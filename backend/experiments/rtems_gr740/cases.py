"""Explicit PolyBench dimensions and independent source-count formulas."""


def workload(kernel, dataset, m=0, n=0, domain=0, repeats=0):
    """Return one case and its inclusive source/loop budgets."""
    if kernel == "atax":
        count, iterations = n + m * (1 + 8 * n), n + m + 2 * m * n
        sizes = {"A": 8 * m * n, "x": 8 * n, "y": 8 * n, "tmp": 8 * m}
    elif kernel == "bicg":
        count, iterations = m + n * (1 + 8 * m), m + n + m * n
        sizes = {"A": 8 * m * n, "s": 8 * m, "q": 8 * n,
                 "p": 8 * m, "r": 8 * n}
    elif kernel == "mvt":
        count, iterations = 8 * n * n, 2 * n + 2 * n * n
        sizes = {"A": 8 * n * n, **{name: 8 * n for name in ("x1", "x2", "y1", "y2")}}
    elif kernel == "sweep":
        count, iterations = 2 * domain * repeats, repeats + domain * repeats
        sizes = {"lines": 32 * domain}
    else:
        raise ValueError(f"unknown kernel: {kernel}")
    return dict(case_id=f"{kernel}-{dataset}", kernel=kernel, dataset=dataset,
                m=m, n=n, domain=domain, repeats=repeats, expected_sources=count,
                symbol_sizes=sizes, working_set_bytes=sum(sizes.values()),
                effective_work_limits=dict(single_loop=max(m, n, domain, repeats),
                                           cumulative_loop=iterations,
                                           source_accesses=count, line_references=count))


def matrix(smoke=False):
    """List micro checks, original dataset dimensions and explicit custom cases."""
    rows = [workload(k, "micro", m=2, n=3) for k in ("atax", "bicg", "mvt")]
    if smoke:
        return rows
    for dataset, m, n, square in (("mini", 38, 42, 40), ("small", 116, 124, 120),
                                  ("medium", 390, 410, 400)):
        rows += [workload(k, dataset, m=m, n=n) for k in ("atax", "bicg")]
        rows.append(workload("mvt", dataset, n=square))
    rows.append(workload("mvt", "llc-crossing-custom", n=520))
    rows += [workload("sweep", f"v4096-r{r}", domain=4096, repeats=r) for r in (1, 16, 128)]
    return rows
