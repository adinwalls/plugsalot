// Plugsalot - GPL-3.0-or-later
#include "Module.h"

namespace plugsalot
{

std::unique_ptr<Module> createMultiCompModule(const ModuleSpec&);
std::unique_ptr<Module> createTapeModule(const ModuleSpec&);
std::unique_ptr<Module> createConsoleEqModule(const ModuleSpec&);
std::unique_ptr<Module> createPlateModule(const ModuleSpec&);
std::unique_ptr<Module> createLimiterModule(const ModuleSpec&);
std::unique_ptr<Module> createDeEssModule(const ModuleSpec&);
std::unique_ptr<Module> createNamAmpModule(const ModuleSpec&);
std::unique_ptr<Module> createCabIrModule(const ModuleSpec&);
std::unique_ptr<Module> createDelayModule(const ModuleSpec&);

std::unique_ptr<Module> createModule(const ModuleSpec& spec)
{
    std::unique_ptr<Module> m;
    if      (spec.id == "multicomp")  m = createMultiCompModule(spec);
    else if (spec.id == "tape")       m = createTapeModule(spec);
    else if (spec.id == "console_eq") m = createConsoleEqModule(spec);
    else if (spec.id == "plate")      m = createPlateModule(spec);
    else if (spec.id == "limiter")    m = createLimiterModule(spec);
    else if (spec.id == "deess")      m = createDeEssModule(spec);
    else if (spec.id == "nam_amp")     m = createNamAmpModule(spec);
    else if (spec.id == "cab_ir")      m = createCabIrModule(spec);
    else if (spec.id == "delay")       m = createDelayModule(spec);
    if (m)
        m->applyUpstreamDefaults();
    return m;
}

} // namespace plugsalot
