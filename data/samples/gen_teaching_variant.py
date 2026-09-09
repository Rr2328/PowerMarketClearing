# -*- coding: utf-8 -*-
"""
生成「现实供需形态教学版」场景变体 scenario_teaching/
（契约 V1.3.1 配套教学数据 · V3）。

动机（老师评审反馈）：
    默认样例 scenario/ 逐时段恰好平衡（±0.02 MW），P4 图上每个时段
    都是"两侧封口竖线重合"——真实市场不会这样，教学效果失真。

设计演变：
    V2（已废弃）：晚峰 69–88 期申报量 ×1.10 —— 只演示稀缺一种形态；
    V3（本版）：**需求系数随负荷连续变化**，一套数据呈现全部三种形态——

        factor(t) = 0.96 + 0.13 × max(0, (负荷(t) − 1000) / (1135.8 − 1000))

      - 常态（负荷 ≤ 1000 MW）：需求 = 供给 × 0.96，留 4% 备用裕度
        → 供大于求、购电侧先满（蓝竖线在左、与供给水平段相交）；
      - 负荷爬升：裕度被逐渐吃掉 → 竖线间距收窄；
      - 早晚双峰（负荷逼近 1135.8）：需求超供给最多 9% → 供给量尽、
        缺口 >> 0.5 MW → 触发契约 V1.3.1 稀缺封顶，出清价 = 540。

    验证结论（Python 对拍复刻引擎逻辑）：
      价格分布 {210:25, 220:28, 230:19, 240:4, 540:20}；
      形态：76 期供大于求 + 20 期供不应求（时段 29–36 早峰、70–81 晚峰）。

用法：
    python gen_teaching_variant.py
    （需要 ../curves/load_curve.csv 计算系数，../scenario/ 为数据源）

可回溯性：
    除购电申报量按上式缩放外，行、列、价格与 scenario/ 完全一致；
    系数公式确定性、可复算。默认 scenario/ 不动（对拍锚点 + 测试基准）。
"""
import csv
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "scenario"
CURVES = HERE / "curves"
DST = HERE / "scenario_teaching"

BASE_FACTOR = 0.96    # 常态需求系数：留 4% 备用裕度（供大于求形态）
PEAK_EXTRA = 0.13     # 峰值附加：负荷顶点时需求超供给约 9%（稀缺形态）
LOAD_T0 = 1000.0      # 起振阈值：负荷超过它才开始吃裕度


def load_curve() -> dict:
    with open(CURVES / "load_curve.csv", encoding="utf-8-sig", newline="") as f:
        return {int(r["时段"]): float(r["负荷(MW)"]) for r in csv.DictReader(f)}


def convert_consumer_csv(src_path: Path, dst_path: Path, load: dict) -> int:
    with open(src_path, encoding="utf-8-sig", newline="") as f:
        rows = list(csv.reader(f))
    header = rows[0]
    period_col = header.index("period")
    qty_cols = [i for i, h in enumerate(header) if "申报量" in h]
    load_max = max(load.values())
    changed = 0
    factors = {}
    for r in rows[1:]:
        if not r:
            continue
        t = int(r[period_col])
        factor = BASE_FACTOR + PEAK_EXTRA * max(0.0, (load[t] - LOAD_T0) / (load_max - LOAD_T0))
        factors[t] = factor
        for i in qty_cols:
            if r[i]:
                r[i] = f"{float(r[i]) * factor:.1f}"
                changed += 1
    with open(dst_path, "w", encoding="utf-8-sig", newline="") as f:
        csv.writer(f).writerows(rows)
    n_peak = sum(1 for v in factors.values() if v > BASE_FACTOR + 1e-9)
    print(f"  需求系数范围 {min(factors.values()):.3f} ~ {max(factors.values()):.3f}"
          f"（吃裕度时段 {n_peak} 个）")
    return changed


def main() -> None:
    DST.mkdir(exist_ok=True)
    load = load_curve()
    n = convert_consumer_csv(SRC / "consumer_bids.csv", DST / "consumer_bids.csv", load)
    # 发电侧原样复制（量价逐字节一致）
    (DST / "generator_bids.csv").write_bytes((SRC / "generator_bids.csv").read_bytes())
    expected = 96 * 3  # 96 期 × 3 用户 × 各 1 段（每期都按系数缩放）
    print(f"已生成 {DST}")
    print(f"申报量按负荷系数缩放：共改 {n} 处（预期 {expected}）")
    assert n == expected, f"预期替换 {expected} 处，实际 {n} 处，请检查！"


if __name__ == "__main__":
    main()
