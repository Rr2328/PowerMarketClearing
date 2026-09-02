# -*- coding: utf-8 -*-
"""
按数据契约 V1.3（D6 样例重标定）生成 scenario 双侧长表样例。

设计口径（与 docs/data-contract.md §八 实施映射一致）：
- 负荷曲线沿用 curves/load_curve.csv（96 点，688.4 ~ 1135.8 MW）
- 购电侧：3 个用户单段申报，基准量之和 = 平均负荷
  → 一键预填 q_k(t) = q_k * w(t)，w(t) = 负荷(t)/平均负荷，逐时段 Σ申报量 = 负荷(t)
- 发电侧：5 台常规机组（金陵电厂 #1/#2、龙潭电厂 #1/#2/#3），无 W/S 行、无机组类型列
  - 基荷段（低价）全天恒量申报：G1s1/G2s1/G1s2/G3s1，合计 460 MW
  - 调峰段（高价）随 residual 出力申报：G2s2/G1s3/G4s1/G3s2/G5s1，
    量 = clamp(thermal_needed(t) - 阈值, 0, 段容量)
  - thermal_needed(t) = Σ购电申报量(t) - 渗透率(20%) * 负荷(t)
    → 默认预填态（渗透率 20%）下逐时段供需恰好平衡
- 期望价格曲线（MCP）：夜谷 210 → 早爬坡 220/230/240 → 早峰 260 → 午间 220 → 晚峰 260 → 夜 210

用法：python gen_scenario_v13.py  （在 data/samples/ 目录下运行，覆盖 scenario/ 两表）
"""
import csv
import os

HERE = os.path.dirname(os.path.abspath(__file__))
LOAD_CSV = os.path.join(HERE, "curves", "load_curve.csv")
GEN_OUT = os.path.join(HERE, "scenario", "generator_bids.csv")
CON_OUT = os.path.join(HERE, "scenario", "consumer_bids.csv")

PENETRATION = 0.20  # 默认渗透率（契约 V1.3：0–100%，默认 20%）


def read_load():
    loads = []
    with open(LOAD_CSV, encoding="utf-8-sig", newline="") as f:
        for row in csv.DictReader(f):
            loads.append(float(row["负荷(MW)"]))
    assert len(loads) == 96, f"负荷曲线应为 96 点，实为 {len(loads)}"
    return loads


def main():
    load = read_load()
    mean_load = sum(load) / len(load)

    # ---- 购电侧：基准量之和 = 平均负荷（预填后逐时段 Σ申报量 = 负荷(t)）----
    # 分配比例：工业园 34% / 商业综合体 40% / 居民小区 26%
    consumers = [
        # (用户名称, 负荷编号, 第1段报价, 基准量)
        ("一号工业园区", "L1", 500.000, round(mean_load * 0.34, 1)),
        ("二号商业综合体", "L2", 380.000, round(mean_load * 0.40, 1)),
        ("三号居民小区", "L3", 280.000, round(mean_load - round(mean_load * 0.34, 1) - round(mean_load * 0.40, 1), 1)),
    ]
    assert abs(sum(c[3] for c in consumers) - mean_load) < 0.15

    w = [v / mean_load for v in load]
    demand = []  # 逐时段 Σ申报量
    for t in range(96):
        demand.append(sum(round(q * w[t], 1) for _, _, _, q in consumers))

    # ---- 发电侧：基荷恒量 + 调峰随爬 ----
    # (电厂名称, 机组编号, [(段价, 基荷恒量 or None, 调峰阈值 or None, 段容量)])
    # 调峰量(t) = clamp(thermal_needed(t) - 阈值, 0, 段容量)
    flat_total = 460.0  # G1s1 150 + G2s1 120 + G1s2 90 + G3s1 100
    gens = [
        ("金陵电厂", "#1机组", [(150.000, 150.0, None, None),   # 基荷
                                (180.000, 90.0, None, None),    # 基荷
                                (220.000, None, 590.0, 90.0)]),  # 调峰：cum(460+130)
        ("金陵电厂", "#2机组", [(170.000, 120.0, None, None),   # 基荷
                                (210.000, None, 460.0, 130.0)]),  # 调峰：cum(460)
        ("龙潭电厂", "#1机组", [(190.000, 100.0, None, None),   # 基荷
                                (240.000, None, 790.0, 80.0)]),   # 调峰：cum(680+110)
        ("龙潭电厂", "#2机组", [(230.000, None, 680.0, 110.0)]),  # 调峰：cum(590+90)
        ("龙潭电厂", "#3机组", [(260.000, None, 870.0, 120.0)]),  # 调峰：cum(790+80)
    ]

    # thermal_needed(t) = Σ购电申报量(t) - 渗透率*负荷(t)
    thermal_needed = [demand[t] - PENETRATION * load[t] for t in range(96)]

    # ---- 写发电长表（3 段成对列）----
    header_g = ["period", "电厂名称", "机组编号",
                "第1段出力(MW)", "第1段报价(元/MWh)",
                "第2段出力(MW)", "第2段报价(元/MWh)",
                "第3段出力(MW)", "第3段报价(元/MWh)"]
    with open(GEN_OUT, "w", encoding="utf-8-sig", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(header_g)
        for t in range(96):
            for plant, unit, segs in gens:
                row = [t + 1, plant, unit]
                for s in range(3):
                    if s < len(segs):
                        price, flat, thresh, cap = segs[s]
                        if flat is not None:
                            qty = flat
                        else:
                            qty = min(max(thermal_needed[t] - thresh, 0.0), cap)
                        row += [f"{qty:.1f}", f"{price:.3f}"]
                    else:
                        row += ["", ""]
                wr.writerow(row)

    # ---- 写购电长表（单段成对列）----
    header_c = ["period", "用户名称", "负荷编号", "第1段申报量(MW)", "第1段报价(元/MWh)"]
    with open(CON_OUT, "w", encoding="utf-8-sig", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(header_c)
        for t in range(96):
            for name, lid, price, q in consumers:
                wr.writerow([t + 1, name, lid, f"{round(q * w[t], 1):.1f}", f"{price:.3f}"])

    # ---- 自检：逐时段阶梯对拍（渗透率 20%）----
    seg_all = []  # (价, 出力函数)
    for plant, unit, segs in gens:
        for price, flat, thresh, cap in segs:
            if flat is not None:
                seg_all.append((price, lambda t, fl=flat: fl))
            else:
                seg_all.append((price, lambda t, th=thresh, cp=cap:
                                min(max(thermal_needed[t] - th, 0.0), cp)))
    print(f"平均负荷 {mean_load:.1f} MW；用户基准量 {[(c[0], c[3]) for c in consumers]}")
    print(f"{'时段':>4} {'时刻':>6} {'负荷':>7} {'需求':>7} {'RENEW':>6} {'火电需':>7} {'出清价':>7} {'边际段'}")
    for t in [0, 9, 31, 51, 74, 95]:  # 夜谷/最低/早峰/午间/晚峰/深夜
        renew = PENETRATION * load[t]
        ladder = sorted(seg_all, key=lambda s: s[0])
        cum = renew
        price, marginal = 0.0, "-"
        for p, fn in ladder:
            q = fn(t)
            if q <= 0:
                continue
            cum += q
            price, marginal = p, f"@{p:.0f}"
            if cum >= demand[t]:
                break
        print(f"{t+1:>4} {t*15//60:02d}:{t*15%60:02d} {load[t]:>7.1f} {demand[t]:>7.1f} "
              f"{renew:>6.1f} {thermal_needed[t]:>7.1f} {price:>7.0f} {marginal}")
    prices = []
    for t in range(96):
        renew = PENETRATION * load[t]
        ladder = sorted(seg_all, key=lambda s: s[0])
        cum, price = renew, 0.0
        for p, fn in ladder:
            q = fn(t)
            if q <= 0:
                continue
            cum += q
            price = p
            if cum >= demand[t]:
                break
        prices.append(price)
    print(f"\n全天价格区间：{min(prices):.0f} ~ {max(prices):.0f} 元/MWh；"
          f"均价 {sum(prices)/96:.1f}；时段数 96")


if __name__ == "__main__":
    main()
