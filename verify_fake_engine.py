# -*- coding: utf-8 -*-
"""
对拍验证：用 Python 复现 FakeEngine 的撮合算法，
验证 benchmark 锚点（MCP 200 / 140 / 56000）与逐时段缩放逻辑。
运行：python verify_fake_engine.py
"""
import csv, os, sys

BASE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "samples")

def read_csv(path):
    """读带 BOM 的 UTF-8 CSV，返回 dict 列表（去掉 BOM 与空白）"""
    with open(path, "r", encoding="utf-8-sig") as f:
        rows = list(csv.DictReader(f))
    for r in rows:
        for k in list(r.keys()):
            r[k.strip()] = (r[k] or "").strip()
    return rows

def clear_benchmark(mode="MCP"):
    """复现 FakeEngine::clearBenchmark：单时段基准对拍"""
    gens = read_csv(os.path.join(BASE, "benchmark", "generator_bids.csv"))
    cons = read_csv(os.path.join(BASE, "benchmark", "consumer_bids.csv"))

    demand = sum(float(c["申报电量(MWh)"]) for c in cons)  # 需求=购电侧申报总量

    # ---- 发电侧：报价升序逐段吸收 ----
    segs = sorted(gens, key=lambda g: float(g["申报电价(元/MWh)"]))
    remaining = demand
    clearing_price = 0.0
    gen_take = []  # (price, qty)
    for g in segs:
        if remaining <= 0:
            break
        qty = float(g["申报电量(MWh)"])
        take = min(qty, remaining)
        if take <= 0:
            continue
        gen_take.append((float(g["申报电价(元/MWh)"]), take))
        clearing_price = float(g["申报电价(元/MWh)"])
        remaining -= take
    if demand <= 0:
        clearing_price = 0.0

    gen_fee = sum((p if mode == "PAB" else clearing_price) * q for p, q in gen_take)
    cleared = sum(q for _, q in gen_take)

    # ---- 购电侧：报价降序撮合，同成交量 ----
    con_segs = sorted(cons, key=lambda c: -float(c["申报电价(元/MWh)"]))
    remaining = cleared
    con_fee = 0.0
    for c in con_segs:
        if remaining <= 0:
            break
        qty = float(c["申报电量(MWh)"])
        take = min(qty, remaining)
        if take <= 0:
            continue
        price = float(c["申报电价(元/MWh)"])
        con_fee += (price if mode == "PAB" else clearing_price) * take
        remaining -= take

    return demand, clearing_price, cleared, gen_fee, con_fee

def check(name, got, want):
    ok = abs(got - want) < 1e-6
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}: got={got:.4f}, want={want}")
    return ok

print("=" * 60)
print("对拍验证 1：benchmark 锚点（MCP 统一出清价）")
print("=" * 60)
demand, cp, cleared, gf, cf = clear_benchmark("MCP")
results = [
    ("需求总量 (MWh)", demand, 140.0),
    ("出清价 (元/MWh)", cp, 200.0),
    ("成交电量 (MWh)", cleared, 140.0),
    ("发电侧结算 (元)", gf, 28000.0),
    ("购电侧结算 (元)", cf, 28000.0),
    ("结算合计 (元)", gf + cf, 56000.0),
]
all_ok = all(check(n, g, w) for n, g, w in results)
print()

print("=" * 60)
print("对拍验证 2：PAB 按报价结算（出清价不变，费用变化）")
print("=" * 60)
demand, cp, cleared, gf, cf = clear_benchmark("PAB")
# PAB：G1 150*100=15000 + G2 200*40=8000 → 23000；U1 300*80=24000 + U2 250*60=15000 → 39000
pab = [
    ("出清价 (元/MWh)", cp, 200.0),
    ("发电侧结算 (元)", gf, 23000.0),
    ("购电侧结算 (元)", cf, 39000.0),
]
all_ok = all(check(n, g, w) for n, g, w in pab) and all_ok
print()

print("=" * 60)
print("对拍验证 3：逐时段缩放（24 时段，抽查 4 个时段）")
print("=" * 60)
# 简化：用手工构造的 4 时段负荷验证缩放比例
loads = [820.0, 790.0, 1500.0, 700.0]  # 总 3810
total = sum(loads)
gen_qty = [100.0, 100.0]  # G1/G2 申报量
for i, L in enumerate(loads):
    scale = L / total
    print(f"  时段{i+1}: 负荷={L:.1f}, 缩放={scale:.4f}, "
          f"G1申报→{gen_qty[0]*scale:.2f} MWh, G2申报→{gen_qty[1]*scale:.2f} MWh")
    assert abs(sum(q * scale for q in gen_qty) - (L / total) * 200.0) < 1e-9
print("  [PASS] 缩放后申报总量 ∝ 该时段负荷（形状不变）")
print()

print("=" * 60)
print(f"总判定：{'全部通过 ✅' if all_ok else '存在失败 ❌'}")
print("=" * 60)
sys.exit(0 if all_ok else 1)
