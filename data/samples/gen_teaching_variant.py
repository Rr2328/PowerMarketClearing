# -*- coding: utf-8 -*-
"""
生成「稀缺教学版」场景变体 scenario_teaching/（契约 V1.3.1 配套教学数据）。

动机（老师评审反馈）：
    默认样例 scenario/ 逐时段恰好平衡，出清价全天走 210~260 五级阶梯，
    永远不触发「供不应求 → 稀缺封顶」——学生看不到价格竖直升顶的教学效果。

设计（V2，替代初版 L3 降价的方案）：
    默认样例 L3 降到 240 只会制造「价格不交叉」（供给嫌贵不卖、需求
    买不到），出清价停在 240，不会触发量尽封顶。因此改为：
      **晚高峰（17:00–22:00，时段 69–88）全部用户申报量 × 1.10**
    ——模拟空调负荷激增。该窗口内需求超过供给申报总量 → 供给量尽、
    缺口 >> 0.5 MW → 触发契约 V1.3.1 稀缺封顶，出清价 = 限价 540；
    窗口外与默认样例完全一致（五级阶梯），一升一平对比鲜明。

    验证结论（Python 对拍复刻引擎逻辑）：540 共 20 个时段（69–88），
    其余 76 期阶梯与默认样例完全一致。

用法：
    python gen_teaching_variant.py

可回溯性：
    除晚峰 20 期的申报量外，所有行、列、数值与 scenario/ 完全一致。
"""
import csv
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "scenario"
DST = HERE / "scenario_teaching"

BUMP = 1.10                 # 晚峰需求上浮系数
PEAK_LO, PEAK_HI = 69, 88   # 17:00–22:00（时段 69–88）


def convert_consumer_csv(src_path: Path, dst_path: Path) -> int:
    with open(src_path, encoding="utf-8-sig", newline="") as f:
        rows = list(csv.reader(f))
    header = rows[0]
    period_col = header.index("period")
    qty_cols = [i for i, h in enumerate(header) if "申报量" in h]
    changed = 0
    for r in rows[1:]:
        if not r:
            continue
        t = int(r[period_col])
        if PEAK_LO <= t <= PEAK_HI:
            for i in qty_cols:
                if r[i]:
                    r[i] = f"{float(r[i]) * BUMP:.1f}"
                    changed += 1
    with open(dst_path, "w", encoding="utf-8-sig", newline="") as f:
        csv.writer(f).writerows(rows)
    return changed


def main() -> None:
    DST.mkdir(exist_ok=True)
    n = convert_consumer_csv(SRC / "consumer_bids.csv", DST / "consumer_bids.csv")
    # 发电侧原样复制（量价逐字节一致）
    data = (SRC / "generator_bids.csv").read_bytes()
    (DST / "generator_bids.csv").write_bytes(data)
    expected = (PEAK_HI - PEAK_LO + 1) * 3  # 晚峰 20 期 × 3 用户 × 各 1 段
    print(f"已生成 {DST}")
    print(f"晚峰时段 {PEAK_LO}–{PEAK_HI} 申报量 ×{BUMP}：共改 {n} 处（预期 {expected}）")
    assert n == expected, f"预期替换 {expected} 处，实际 {n} 处，请检查！"


if __name__ == "__main__":
    main()
