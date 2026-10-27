"""Parameter extractors, one per upstream source format.

Each module exposes `extract(root, cfg)` (or `extract(root, cfg, roots)` when it
sets NEEDS_ROOTS = True) and returns a dict with at least a "params" list.
"""

from . import airwindows, dpf_param_table, dusk_daf, dusk_table, juce_apvts, lsp_para_eq, lv2_ttl

EXTRACTORS = {
    "airwindows": airwindows,
    "dpf_param_table": dpf_param_table,
    "dusk_daf": dusk_daf,
    "dusk_table": dusk_table,
    "juce_apvts": juce_apvts,
    "lsp_para_eq": lsp_para_eq,
    "lv2_ttl": lv2_ttl,
}
