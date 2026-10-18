// #####################################################################
//  Immersived HUD — plugin ASI para GTA San Andreas PC 1.0 US
//  Versión: 0.1.0-dev.1 — proyecto mínimo
//  Requiere: plugin-sdk (Plugin_SA) y un cargador de ASI.
//  Repositorio: https://github.com/PulpoScript/gta-sa-immersived-hud
// #####################################################################
//
//  CONTROLES
//    F11  Enciende / apaga el plugin (muestra un mensaje en pantalla).
//
//  Por ahora el plugin no cambia nada del HUD: solo comprueba que el
//  proyecto compila, que el juego carga el .asi y que lee el teclado.
// #####################################################################

#include "plugin.h"
#include "CHud.h"

using namespace plugin;

// =====================================================================
//  CONFIGURACIÓN
// =====================================================================
static const char* VERSION = "0.1.0-dev.1";
static const int   TECLA_ACTIVAR = VK_F11;

// =====================================================================
//  PLUGIN
// =====================================================================
class ImmersivedHud {
public:
    ImmersivedHud() {
        // Se ejecuta una vez por frame mientras se juega.
        Events::gameProcessEvent += [] {
            static bool activo = false;       // estado del plugin
            static bool teclaAntes = false;   // F11 pulsada en el frame anterior

            // ---- Tecla F11: cambia de estado solo al pulsar (no al mantener) ----
            bool teclaAhora = (GetAsyncKeyState(TECLA_ACTIVAR) & 0x8000) != 0;
            if (teclaAhora && !teclaAntes) {
                activo = !activo;

                // ---- Aviso en pantalla (sin acentos: la fuente del juego no los tiene) ----
                static char texto[64];
                sprintf(texto, "Immersived HUD v%s: %s", VERSION, activo ? "activado" : "desactivado");
                CHud::SetHelpMessage(texto, true, false, false);
            }
            teclaAntes = teclaAhora;
        };
    }
} immersivedHud;
