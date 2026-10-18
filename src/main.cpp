// #####################################################################
//  Immersived HUD — plugin ASI para GTA San Andreas PC 1.0 US
//  Versión: 0.1.0-dev.3 — primer movimiento: el dinero
//  Requiere: plugin-sdk (Plugin_SA) y un cargador de ASI.
//  Repositorio: https://github.com/PulpoScript/gta-sa-immersived-hud
// #####################################################################
//
//  CONTROLES
//    F11  Enciende / apaga el plugin (muestra un mensaje en pantalla).
//         Encendido: el dinero se mueve a una posición de prueba.
//         Apagado: el dinero vuelve a su posición de fábrica.
//    F10  Escribe ImmersivedHud.log junto a gta_sa.exe con lo que hay en
//         las direcciones candidatas a posición del HUD (solo lectura).
//
//  TÉCNICA (la de WidescreenFix)
//    Cada coordenada se lee con una instrucción "fmul [puntero]". No se
//    toca la constante del juego (algunas se comparten entre elementos):
//    se cambia el puntero de la instrucción para que apunte a un float
//    propio. Antes de escribir se comprueba que el puntero sea el original.
// #####################################################################

#include "plugin.h"
#include "CHud.h"
#include <cstdio>
#include <cstring>

using namespace plugin;

// =====================================================================
//  CONFIGURACIÓN
// =====================================================================
static const char* VERSION = "0.1.0-dev.3";
static const int   TECLA_ACTIVAR = VK_F11;
static const int   TECLA_DIAGNOSTICO = VK_F10;

// =====================================================================
//  DIRECCIONES A REVISAR (SIN VERIFICAR)
//  Cada una debería ser el operando de 4 bytes de una instrucción que
//  lee un float (por ejemplo fmul [mem] = D8 0D, fld [mem] = D9 05).
//  Fuentes: listas de foros (ugbase.eu, blast.hk) y WidescreenFix
//  (Legacy.ixx). Ver docs/INVESTIGACION.md del mod CLEO.
// =====================================================================
struct Candidata {
    const char* nombre;
    unsigned int direccion;
};

static const Candidata CANDIDATAS[] = {
    // ---- Punteros de posición (listas de foros) ----
    { "Dinero X",             0x58F5FC },
    { "Dinero Y",             0x58F5DC },
    { "Salud X",              0x58EE87 },
    { "Salud Y",              0x58EE68 },
    { "Blindaje X",           0x58EF59 },
    { "Blindaje Y",           0x58EF3A },
    { "Oxigeno X",            0x58F11F },
    { "Estrellas X",          0x58DD0F },
    { "Estrellas Y",          0x58DDFC },
    { "Arma X (a)",           0x58F927 },
    { "Arma X (b)",           0x58F92F },  // WidescreenFix lo redirige
    { "Municion X",           0x58F9F7 },  // WidescreenFix lo redirige
    { "Municion Y",           0x58F9DC },

    // ---- Instrucciones de escala (WidescreenFix), operando en +2 ----
    { "Hora ancho 1",         0x58EB3F + 2 },
    { "Hora ancho 2",         0x58EC0C + 2 },
    { "Hora alto 1",          0x58EB29 + 2 },
    { "Hora alto 2",          0x58EBF9 + 2 },
    { "Dinero ancho 1",       0x58F55C + 2 },
    { "Dinero ancho 2",       0x58F5F4 + 2 },
    { "Dinero alto 1",        0x58F546 + 2 },
    { "Dinero alto 2",        0x58F5CE + 2 },
};

// =====================================================================
//  LECTURA SEGURA
// =====================================================================
// Devuelve true si se pueden leer 'tam' bytes en 'dir' sin fallo.
static bool SePuedeLeer(unsigned int dir, unsigned int tam) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(dir), &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    unsigned int fin = reinterpret_cast<unsigned int>(mbi.BaseAddress) + mbi.RegionSize;
    return dir + tam <= fin;
}

// =====================================================================
//  REDIRECCIÓN DE PUNTEROS
// =====================================================================
// Un puntero de coordenada: dónde está, a qué apunta de fábrica y el
// float propio al que se redirige.
struct Coordenada {
    unsigned int direccion;    // operando de 4 bytes de la instrucción
    unsigned int original;     // puntero de fábrica (verificado en dev.2)
    float valor;               // valor propio (unidades del HUD)
    bool redirigida;
};

// Escribe 4 bytes en el código del juego. Devuelve true si se pudo.
static bool EscribirPuntero(unsigned int dir, unsigned int valor) {
    DWORD antes;
    void* p = reinterpret_cast<void*>(dir);
    if (!VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &antes)) return false;
    *reinterpret_cast<unsigned int*>(dir) = valor;
    VirtualProtect(p, 4, antes, &antes);
    return true;
}

// Apunta la instrucción al float propio, solo si el puntero es el de fábrica.
static void Redirigir(Coordenada& c) {
    if (c.redirigida) return;
    if (*reinterpret_cast<const unsigned int*>(c.direccion) != c.original) return;
    c.redirigida = EscribirPuntero(c.direccion, reinterpret_cast<unsigned int>(&c.valor));
}

// Devuelve la instrucción a su puntero de fábrica, solo si lo cambiamos nosotros.
static void Restaurar(Coordenada& c) {
    if (!c.redirigida) return;
    if (*reinterpret_cast<const unsigned int*>(c.direccion) != reinterpret_cast<unsigned int>(&c.valor)) return;
    if (EscribirPuntero(c.direccion, c.original)) c.redirigida = false;
}

// ---- Dinero (verificado en dev.2: fábrica X = 32, Y = 89) ----
static Coordenada dineroX = { 0x58F5FC, 0x0085950C, 100.0f, false };
static Coordenada dineroY = { 0x58F5DC, 0x00866C88, 140.0f, false };

// =====================================================================
//  DIAGNÓSTICO
// =====================================================================
// Escribe el archivo junto a gta_sa.exe. Devuelve true si se pudo.
static bool EscribirDiagnostico() {
    char ruta[MAX_PATH];
    GetModuleFileNameA(NULL, ruta, MAX_PATH);
    char* barra = strrchr(ruta, '\\');
    if (barra) strcpy(barra + 1, "ImmersivedHud.log");
    else strcpy(ruta, "ImmersivedHud.log");

    FILE* f = fopen(ruta, "w");
    if (!f) return false;

    fprintf(f, "Immersived HUD v%s - diagnostico (solo lectura)\n", VERSION);
    fprintf(f, "Columnas: nombre | direccion | bytes desde dir-2 | puntero | float apuntado\n\n");

    for (const Candidata& c : CANDIDATAS) {
        fprintf(f, "%-16s 0x%06X  ", c.nombre, c.direccion);

        // ---- Bytes alrededor: 2 antes (código de instrucción) y 4 del operando ----
        if (!SePuedeLeer(c.direccion - 2, 6)) {
            fprintf(f, "no se puede leer\n");
            continue;
        }
        const unsigned char* b = reinterpret_cast<const unsigned char*>(c.direccion - 2);
        fprintf(f, "%02X %02X [%02X %02X %02X %02X]  ", b[0], b[1], b[2], b[3], b[4], b[5]);

        // ---- Puntero y float al que apunta ----
        unsigned int puntero = *reinterpret_cast<const unsigned int*>(c.direccion);
        fprintf(f, "-> 0x%08X  ", puntero);
        if (SePuedeLeer(puntero, 4)) {
            float valor = *reinterpret_cast<const float*>(puntero);
            fprintf(f, "= %.6f\n", valor);
        } else {
            fprintf(f, "(no apunta a memoria legible)\n");
        }
    }

    fclose(f);
    return true;
}

// =====================================================================
//  PLUGIN
// =====================================================================
class ImmersivedHud {
public:
    ImmersivedHud() {
        // Se ejecuta una vez por frame mientras se juega.
        Events::gameProcessEvent += [] {
            static bool activo = false;          // estado del plugin
            static bool teclaAntes = false;      // F11 pulsada en el frame anterior
            static bool diagAntes = false;       // F10 pulsada en el frame anterior
            static char texto[96];

            // ---- Tecla F11: cambia de estado solo al pulsar (no al mantener) ----
            bool teclaAhora = (GetAsyncKeyState(TECLA_ACTIVAR) & 0x8000) != 0;
            if (teclaAhora && !teclaAntes) {
                activo = !activo;

                // ---- Mover o devolver el dinero ----
                if (activo) { Redirigir(dineroX); Redirigir(dineroY); }
                else        { Restaurar(dineroX); Restaurar(dineroY); }

                // ---- Aviso en pantalla (sin acentos: la fuente del juego no los tiene) ----
                sprintf(texto, "Immersived HUD v%s: %s", VERSION, activo ? "activado" : "desactivado");
                CHud::SetHelpMessage(texto, true, false, false);
            }
            teclaAntes = teclaAhora;

            // ---- Tecla F10: diagnóstico de direcciones (solo lectura) ----
            bool diagAhora = (GetAsyncKeyState(TECLA_DIAGNOSTICO) & 0x8000) != 0;
            if (diagAhora && !diagAntes) {
                bool ok = EscribirDiagnostico();
                sprintf(texto, ok ? "Diagnostico guardado en ImmersivedHud.log"
                                  : "No se pudo escribir ImmersivedHud.log");
                CHud::SetHelpMessage(texto, true, false, false);
            }
            diagAntes = diagAhora;
        };
    }
} immersivedHud;
