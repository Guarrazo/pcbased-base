#include "graphics/gl_shim/gl_shim.h"
#include "core/log.h"

namespace pas::graphics {

GlShim::GlShim(IGraphicsBackend& backend) : backend_(backend) {}

void GlShim::RegisterSymbols() {
    // TODO(Nivel 1, docs/ROADMAP.md): registrar los simbolos gl*/cg*
    // realmente usados por el titulo elegido -- se descubren desde la lista
    // de UnresolvedSymbol que reporta ElfLoader::Load(), no de antemano.
    PAS_LOG_WARN("GlShim", "RegisterSymbols no implementado todavia");
}

} // namespace pas::graphics
