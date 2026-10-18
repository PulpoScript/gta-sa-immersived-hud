// #####################################################################
//  Immersived HUD — plugin ASI para GTA San Andreas PC 1.0 US
//  Versión: 0.1.0-dev.12 — barras un 10 % más pequeñas
//  Requiere: plugin-sdk (Plugin_SA) y un cargador de ASI.
//  Repositorio: https://github.com/PulpoScript/gta-sa-immersived-hud
// #####################################################################
//
//  CONTROLES
//    F11  Enciende / apaga el plugin (muestra un mensaje en pantalla).
//         Encendido: el HUD pasa al montaje compacto (icono en la
//         esquina; a su izquierda hora, dinero, blindaje y salud;
//         estrellas debajo).
//         Apagado: todo vuelve a su posición de fábrica.
//    F10  Escribe ImmersivedHud.log junto a gta_sa.exe con lo que hay en
//         las direcciones candidatas a posición del HUD y una búsqueda de
//         instrucciones de float en el código de la hora, el icono, la
//         munición, las estrellas, el dinero, las barras y DrawBarChart,
//         dónde está el contorno de la hora y el dinero, y un volcado en
//         hexadecimal de DrawBarChart (solo lectura).
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
static const char* VERSION = "0.1.0-dev.12";
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

// ---- Rangos de código donde buscar posiciones ----
struct Rango {
    const char* nombre;
    unsigned int inicio;
    unsigned int fin;
};

static const Rango BUSQUEDAS[] = {
    { "hora",              0x58EA80, 0x58EC80 },  // alrededor de las escalas de la hora
    { "icono y municion",  0x58F880, 0x58FAC0 },  // alrededor de las escalas del icono y la munición
    { "estrellas",         0x58DC80, 0x58E020 },  // alrededor de las escalas de las estrellas
    { "dinero",            0x58F500, 0x58F620 },  // alrededor de las escalas del dinero
    { "salud y blindaje",  0x58EE00, 0x58F000 },  // alrededor de las escalas de las barras
    { "dibujo del icono",  0x58D840, 0x58D980 },  // alrededor de las escalas del dibujo del icono
    { "DrawBarChart",      0x728640, 0x728900 },  // CSprite2d::DrawBarChart: borde negro de las barras
};

// ---- Rango a volcar en hexadecimal: CSprite2d::DrawBarChart ----
static const unsigned int VOLCADO_INICIO = 0x728640;
static const unsigned int VOLCADO_FIN    = 0x728A40;

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

// =====================================================================
//  MONTAJE COMPACTO
//  Unidades del HUD (con WidescreenFix a 1080p, 1 unidad ≈ 1,93 px).
//  X se mide desde el borde derecho; Y desde arriba.
//  Punteros de fábrica verificados en dev.2 y dev.4.
//  Valores: primera aproximación sacada de las capturas; hay que ajustar.
// =====================================================================
static Coordenada MONTAJE[] = {
    // ---- Icono de arma: pegado a la esquina (fábrica X 32, Y 20) ----
    { 0x58F927, 0x0085950C, -57.0f, false },  // X = ancho - (esto + parte de WidescreenFix)
    { 0x58F913, 0x00858BA4,   4.5f, false },  // Y
    // ---- Munición: sigue al icono, sobre su parte de abajo (fábrica X 32, Y 20 + 43) ----
    { 0x58FA02, 0x0085950C, -57.0f, false },
    { 0x58F9DC, 0x00858BA4,   4.5f, false },  // Y base
    // ---- Hora: a la izquierda del icono, letra más pequeña (fábrica X 32, Y 22, letra 1,1 / 0,55) ----
    { 0x58EC16, 0x0085950C,  57.0f, false },
    { 0x58EC04, 0x00858F8C,  12.0f, false },
    { 0x58EB31, 0x00858F14,  0.64f, false },  // alto de letra
    { 0x58EB47, 0x00866CAC,  0.32f, false },  // ancho de letra
    // ---- Dinero: debajo de la hora, misma letra (fábrica X 32, Y 89, letra 1,1 / 0,55; el juego resta 12 a la Y si la salud máxima < 101) ----
    { 0x58F5FC, 0x0085950C,  57.0f, false },
    { 0x58F5DC, 0x00866C88,  38.5f, false },
    { 0x58F54E, 0x00858F14,  0.64f, false },  // alto de letra (sin probar)
    { 0x58F564, 0x00866CAC,  0.32f, false },  // ancho de letra (sin probar)
    // ---- Blindaje: debajo del dinero (fábrica X 94, Y 48) ----
    { 0x58EF59, 0x00866B78, 119.0f, false },
    { 0x58EF3A, 0x00862D38,  43.0f, false },
    // ---- Salud: debajo del blindaje, termina a la altura del icono (fábrica X 141, Y 77) ----
    { 0x58EE87, 0x0086535C, 166.0f, false },
    { 0x58EE68, 0x00866CA8,  60.0f, false },
    // ---- Estrellas: debajo de la salud, alineadas con el icono (fábrica X 29, Y 114) ----
    { 0x58DD0F, 0x00863210,   8.0f, false },
    { 0x58DDFC, 0x00866C5C,  72.0f, false },  // encendidas (fld)
    { 0x58DEF5, 0x00866C5C,  72.0f, false },  // apagadas
    { 0x58DFB3, 0x00866C5C,  72.0f, false },  // otra pasada con la misma Y
};

// =====================================================================
//  CONTORNO DE LA HORA Y EL DINERO
//  Ambos llaman a CFont::SetEdge(2) (gta-reversed, Hud.cpp). El valor
//  es un entero, así que lo más fino sin quitarlo es 1.
//  Se busca el patrón "push 2; call CFont::SetEdge" dentro del código de
//  cada elemento y solo se cambia el byte del 2 si aparece una única vez.
// =====================================================================
static const unsigned int FUNCION_SET_EDGE = 0x719590;  // CFont::SetEdge (plugin-sdk)
static const unsigned char CONTORNO_FABRICA = 2;
static const unsigned char CONTORNO_PROPIO  = 1;

struct Contorno {
    const char* nombre;
    unsigned int inicio;     // rango del código donde buscar
    unsigned int fin;
    unsigned int byteValor;  // dirección del 2 encontrada (0 = sin buscar o no válida)
    bool cambiado;
};

static Contorno CONTORNOS[] = {
    { "hora",   0x58EA80, 0x58EC80, 0, false },
    { "dinero", 0x58F500, 0x58F6A0, 0, false },
};

// Cuenta las apariciones de "6A 02 E8 <rel32 a SetEdge>" en el rango y
// devuelve la dirección del byte del valor si hay exactamente una.
static unsigned int BuscarSetEdge(const Contorno& c, int* encontrados) {
    *encontrados = 0;
    unsigned int resultado = 0;
    if (!SePuedeLeer(c.inicio, c.fin - c.inicio + 7)) return 0;
    for (unsigned int dir = c.inicio; dir < c.fin; dir++) {
        const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
        if (b[0] != 0x6A || b[1] != CONTORNO_FABRICA || b[2] != 0xE8) continue;
        int rel = *reinterpret_cast<const int*>(dir + 3);
        if (dir + 7 + rel != FUNCION_SET_EDGE) continue;
        (*encontrados)++;
        resultado = dir + 1;
    }
    return (*encontrados == 1) ? resultado : 0;
}

static bool EscribirByte(unsigned int dir, unsigned char valor) {
    DWORD antes;
    void* p = reinterpret_cast<void*>(dir);
    if (!VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &antes)) return false;
    *reinterpret_cast<unsigned char*>(dir) = valor;
    VirtualProtect(p, 1, antes, &antes);
    return true;
}

static void AplicarContornos() {
    for (Contorno& c : CONTORNOS) {
        if (c.cambiado) continue;
        int encontrados;
        c.byteValor = BuscarSetEdge(c, &encontrados);
        if (c.byteValor == 0) continue;  // no está o está repetido: no se toca
        c.cambiado = EscribirByte(c.byteValor, CONTORNO_PROPIO);
    }
}

static void QuitarContornos() {
    for (Contorno& c : CONTORNOS) {
        if (!c.cambiado) continue;
        if (*reinterpret_cast<const unsigned char*>(c.byteValor) != CONTORNO_PROPIO) continue;
        if (EscribirByte(c.byteValor, CONTORNO_FABRICA)) c.cambiado = false;
    }
}

// =====================================================================
//  TAMAÑO DE LAS BARRAS
//  gta-reversed (Hud.cpp): RenderArmorBar (0x58A160) usa 62 × 9 y
//  RenderHealthBar (0x589270) usa 109 × 9. La dirección exacta no está
//  verificada: se busca dentro de cada función una instrucción fmul/fld
//  que lea ese valor y solo se redirige si aparece una única vez.
// =====================================================================
struct Tamano {
    const char* nombre;
    unsigned int inicio;     // rango del código donde buscar
    unsigned int fin;
    float fabrica;           // valor que debe leer la instrucción
    int encontrados;         // resultado de la última búsqueda
    Coordenada c;            // dirección y puntero se rellenan al encontrarla
};

static Tamano TAMANOS[] = {
    { "blindaje largo", 0x58A160, 0x58A230,  62.0f, 0, { 0, 0, 56.0f, false } },
    { "blindaje alto",  0x58A160, 0x58A230,   9.0f, 0, { 0, 0,  8.0f, false } },
    { "salud largo",    0x589270, 0x5893B0, 109.0f, 0, { 0, 0, 98.0f, false } },
    { "salud alto",     0x589270, 0x5893B0,   9.0f, 0, { 0, 0,  8.0f, false } },
};

// Busca "fmul [mem]" (D8 0D) o "fld [mem]" (D9 05) cuyo float valga 'valor'.
static void BuscarTamano(Tamano& t) {
    t.encontrados = 0;
    if (!SePuedeLeer(t.inicio, t.fin - t.inicio + 6)) return;
    for (unsigned int dir = t.inicio; dir < t.fin; dir++) {
        const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
        bool esFloat = (b[0] == 0xD8 && b[1] == 0x0D) || (b[0] == 0xD9 && b[1] == 0x05);
        if (!esFloat) continue;
        unsigned int puntero = *reinterpret_cast<const unsigned int*>(dir + 2);
        if (!SePuedeLeer(puntero, 4)) continue;
        if (*reinterpret_cast<const float*>(puntero) != t.fabrica) continue;
        t.encontrados++;
        t.c.direccion = dir + 2;
        t.c.original = puntero;
    }
}

// Devuelve la coordenada del montaje que usa esa dirección.
static Coordenada* BuscarEnMontaje(unsigned int direccion) {
    for (Coordenada& c : MONTAJE) if (c.direccion == direccion) return &c;
    return NULL;
}

// Margen derecho del grupo: el borde derecho de las barras queda aquí.
static const float BORDE_DERECHO = 57.0f;

static void AplicarTamanos() {
    for (Tamano& t : TAMANOS) {
        if (t.c.redirigida) continue;
        BuscarTamano(t);
        if (t.encontrados == 1) Redirigir(t.c);
    }
    // ---- La X de cada barra es su borde izquierdo: se ajusta al largo real ----
    Coordenada* blindajeX = BuscarEnMontaje(0x58EF59);
    Coordenada* saludX = BuscarEnMontaje(0x58EE87);
    if (blindajeX) blindajeX->valor = BORDE_DERECHO + (TAMANOS[0].c.redirigida ? TAMANOS[0].c.valor : 62.0f);
    if (saludX)    saludX->valor    = BORDE_DERECHO + (TAMANOS[2].c.redirigida ? TAMANOS[2].c.valor : 109.0f);
}

static void QuitarTamanos() {
    for (Tamano& t : TAMANOS) Restaurar(t.c);
}

static void AplicarMontaje() {
    for (Coordenada& c : MONTAJE) Redirigir(c);
    AplicarContornos();
    AplicarTamanos();
}

static void QuitarMontaje() {
    for (Coordenada& c : MONTAJE) Restaurar(c);
    QuitarContornos();
    QuitarTamanos();
}

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

    // ---- Búsqueda de instrucciones de float ----
    // Instrucciones x87 con operando de memoria directo:
    //   D8 05 fadd, D8 0D fmul, D8 25 fsub, D8 2D fsubr, D8 35 fdiv,
    //   D8 3D fdivr, D9 05 fld. Solo se anotan si el puntero es legible.
    for (const Rango& r : BUSQUEDAS) {
        fprintf(f, "\nBusqueda 0x%06X-0x%06X (%s)\n", r.inicio, r.fin, r.nombre);
        fprintf(f, "Columnas: direccion del operando | instruccion | puntero | float apuntado\n\n");
        if (!SePuedeLeer(r.inicio, r.fin - r.inicio + 6)) {
            fprintf(f, "no se puede leer el rango\n");
            continue;
        }
        for (unsigned int dir = r.inicio; dir < r.fin; dir++) {
            const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
            const char* nombre = NULL;
            if (b[0] == 0xD8) {
                switch (b[1]) {
                    case 0x05: nombre = "fadd "; break;
                    case 0x0D: nombre = "fmul "; break;
                    case 0x25: nombre = "fsub "; break;
                    case 0x2D: nombre = "fsubr"; break;
                    case 0x35: nombre = "fdiv "; break;
                    case 0x3D: nombre = "fdivr"; break;
                }
            } else if (b[0] == 0xD9 && b[1] == 0x05) {
                nombre = "fld  ";
            }
            if (!nombre) continue;

            unsigned int puntero = *reinterpret_cast<const unsigned int*>(dir + 2);
            if (!SePuedeLeer(puntero, 4)) continue;
            float valor = *reinterpret_cast<const float*>(puntero);
            fprintf(f, "0x%06X  %s -> 0x%08X = %.6f\n", dir + 2, nombre, puntero, valor);
        }
    }

    // ---- Tamaño de las barras ----
    fprintf(f, "\nTamano de las barras\n\n");
    for (Tamano& t : TAMANOS) {
        if (t.c.redirigida) {
            fprintf(f, "%-15s cambiado a %.1f en 0x%06X (fabrica %.1f)\n", t.nombre, t.c.valor, t.c.direccion, t.fabrica);
            continue;
        }
        BuscarTamano(t);
        fprintf(f, "%-15s encontrados: %d", t.nombre, t.encontrados);
        if (t.encontrados == 1) fprintf(f, "  en 0x%06X", t.c.direccion);
        fprintf(f, "\n");
    }

    // ---- Contorno de la hora y el dinero ----
    fprintf(f, "\nContorno (push 2; call CFont::SetEdge 0x%06X)\n\n", FUNCION_SET_EDGE);
    for (const Contorno& c : CONTORNOS) {
        if (c.cambiado) {
            fprintf(f, "%-8s cambiado a %d en 0x%06X\n", c.nombre, CONTORNO_PROPIO, c.byteValor);
            continue;
        }
        int encontrados;
        unsigned int dir = BuscarSetEdge(c, &encontrados);
        fprintf(f, "%-8s encontrados: %d", c.nombre, encontrados);
        if (dir) fprintf(f, "  valor en 0x%06X", dir);
        fprintf(f, "\n");
    }

    // ---- Volcado de CSprite2d::DrawBarChart (para desensamblar fuera) ----
    fprintf(f, "\nVolcado 0x%06X-0x%06X (DrawBarChart)\n\n", VOLCADO_INICIO, VOLCADO_FIN);
    if (SePuedeLeer(VOLCADO_INICIO, VOLCADO_FIN - VOLCADO_INICIO)) {
        for (unsigned int dir = VOLCADO_INICIO; dir < VOLCADO_FIN; dir += 16) {
            const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
            fprintf(f, "%06X:", dir);
            for (int i = 0; i < 16; i++) fprintf(f, " %02X", b[i]);
            fprintf(f, "\n");
        }
    } else {
        fprintf(f, "no se puede leer el rango\n");
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

                // ---- Aplicar o quitar el montaje compacto ----
                if (activo) AplicarMontaje();
                else        QuitarMontaje();

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
