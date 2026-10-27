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

std::unique_ptr<Module> createModule(const ModuleSpec& spec)
{
    std::unique_ptr<Module> m;
    if      (spec.id == "multicomp")  m = createMultiCompModule(spec);
    else if (spec.id == "tape")       m = createTapeModule(spec);
    else if (spec.id == "console_eq") m = createConsoleEqModule(spec);
    else if (spec.id == "plate")      m = createPlateModule(spec);
    else if (spec.id == "limiter")    m = createLimiterModule(spec);
    else if (spec.id == "deess")      m = createDeEssModule(spec);
    if (m)
        m->applyUpstreamDefaults();
    return m;
}

} // namespace plugsalot
