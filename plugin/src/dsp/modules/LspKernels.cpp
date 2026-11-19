// Plugsalot - GPL-3.0-or-later
// The few lsp-dsp-lib kernels that the vendored LSP filter code calls, bound to the library's
// portable generic C++ implementations (no assembly, no CPU dispatch). Upstream selects these at
// run time through function pointers; here they are fixed at compile time.
#include <lsp-plug.in/common/bits.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/stdlib/math.h>
#include <lsp-plug.in/stdlib/string.h>
#include <stdlib.h>

namespace lsp { using namespace dsp; }

#define PRIVATE_DSP_ARCH_GENERIC_IMPL
#include <private/dsp/arch/generic/copy.h>
#include <private/dsp/arch/generic/pcomplex.h>
#include <private/dsp/arch/generic/pmath/op_kx.h>
#include <private/dsp/arch/generic/filters.h>
#undef PRIVATE_DSP_ARCH_GENERIC_IMPL

namespace lsp
{
    namespace dsp
    {
        #define PLUGSALOT_LSP_KERNEL(name) decltype(name) name = lsp::generic::name;

        PLUGSALOT_LSP_KERNEL(biquad_pack_x2)
        PLUGSALOT_LSP_KERNEL(biquad_pack_x4)
        PLUGSALOT_LSP_KERNEL(biquad_pack_x8)
        PLUGSALOT_LSP_KERNEL(biquad_pack_x16)
        PLUGSALOT_LSP_KERNEL(biquad_process_x1)
        PLUGSALOT_LSP_KERNEL(biquad_process_x2)
        PLUGSALOT_LSP_KERNEL(biquad_process_x4)
        PLUGSALOT_LSP_KERNEL(biquad_process_x8)
        PLUGSALOT_LSP_KERNEL(biquad_process_x16)
        PLUGSALOT_LSP_KERNEL(copy)
        PLUGSALOT_LSP_KERNEL(fill_one)
        PLUGSALOT_LSP_KERNEL(fill_zero)
        PLUGSALOT_LSP_KERNEL(mul_k3)
        PLUGSALOT_LSP_KERNEL(pcomplex_fill_ri)
        PLUGSALOT_LSP_KERNEL(filter_transfer_apply_pc)
        PLUGSALOT_LSP_KERNEL(filter_transfer_apply_ri)
        PLUGSALOT_LSP_KERNEL(filter_transfer_calc_pc)
        PLUGSALOT_LSP_KERNEL(filter_transfer_calc_ri)
    }
}
