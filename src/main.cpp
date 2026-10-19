// #####################################################################
//  Immersived HUD — plugin ASI para GTA San Andreas PC 1.0 US
//  Versión: 0.2.0-dev.7 — opacidad del HUD y menú F9 con subpáginas
//  Requiere: plugin-sdk (Plugin_SA) y un cargador de ASI.
//  Repositorio: https://github.com/PulpoScript/gta-sa-immersived-hud
// #####################################################################
//
//  QUÉ HACE
//    El HUD va en un montaje compacto arriba a la derecha (icono en la
//    esquina; a su izquierda hora, dinero, blindaje y salud; estrellas
//    debajo; oxígeno abajo a la derecha). Cada elemento aparece cuando
//    pasa algo y se oculta al rato; lo oculto deja su hueco y el resto
//    se recoloca. Las reglas son las del Modo 2 del mod CLEO. Debajo del
//    dinero se ve lo ganado (+250, verde) o perdido (-540, rojo).
//
//  CONTROLES
//    "."  Toque corto: la hora 5 s. Mantener 0,8 s: todo el HUD visible
//         (en el montaje compacto); al soltar, se oculta a los 3 s.
//    F11  Apaga / enciende el plugin (empieza encendido). Apagado: HUD
//         de fábrica.
//    F10  Escribe ImmersivedHud.log junto a gta_sa.exe (diagnóstico,
//         solo lectura).
//    F9   Menú de depuración (temporal, se quitará en la versión final):
//         flechas arriba / abajo para elegir, izquierda / derecha para
//         cambiar, Intro para "restaurar". Se guarda en ImmersivedHud.ini.
//
//  TÉCNICA
//    Posición y tamaño: cada coordenada se lee con "fmul [puntero]". No
//    se toca la constante del juego (algunas se comparten): se cambia el
//    puntero de la instrucción para que apunte a un float propio, como
//    hace WidescreenFix. Ocultar: parches "ret" / NOP probados en el mod
//    CLEO. Antes de escribir se comprueba siempre el valor original.
// #####################################################################

#include "plugin.h"
#include "CHud.h"
#include "CWorld.h"
#include "CTimer.h"
#include "CPad.h"
#include "CGameLogic.h"
#include "CFont.h"
#include "CHudColours.h"
#include "CMenuManager.h"
#include "CTheScripts.h"
#include "CCamera.h"
#include "CReplay.h"
#include "CWeapon.h"
#include "CRadar.h"
#include "CSprite2d.h"
#include "CStats.h"
#include <cstdio>
#include <cstring>
#include <cmath>

using namespace plugin;

// =====================================================================
//  CONFIGURACIÓN
// =====================================================================
static const char* VERSION = "0.2.0-dev.7";
static const int   TECLA_ACTIVAR = VK_F11;
static const int   TECLA_DIAGNOSTICO = VK_F10;
static const int   TECLA_CONSULTA = VK_OEM_PERIOD;   // "." (no la del teclado numérico)

static const int   TECLA_MENU = VK_F9;
static const unsigned int PULSACION_LARGA = 800;   // ms para que "." muestre todo

// =====================================================================
//  AJUSTES (se cambian con el menú F9 y se guardan en ImmersivedHud.ini)
// =====================================================================
enum Modo { MODO_DINAMICO = 0, MODO_SIEMPRE = 1, MODO_NUNCA = 2 };
enum FormaRadar { RADAR_REDONDO = 0, RADAR_CUADRADO = 1, RADAR_3_2 = 2, RADAR_16_9 = 3 };
enum Elemento { E_HORA, E_DINERO, E_ARMA, E_SALUD, E_BLINDAJE, E_OXIGENO, E_RESISTENCIA, E_ESTRELLAS, NUM_ELEMENTOS };

struct Ajustes {
    int montaje;            // 1 = montaje compacto, 0 = posiciones de fábrica
    int diferencia;         // 1 = mostrar +250 / -540 bajo el dinero
    int modoDinero, modoArma, modoBarras, modoHora, modoRadar;   // Modo
    int msDinero, msArma, msBarras, msHora, msRadar, msTodo;     // ms visibles
    int saludBaja;          // puntos: por debajo, barras fijas
    int radarVehiculo, radarEsprintar, radarEstrellas, radarObjetivo, radarDestino;
    int modoResistencia, msResistencia;   // barra de resistencia
    int bloquearCJ;         // 1 = CJ no se mueve con el menú F9 abierto
    int formaRadar;         // FormaRadar
    int opacidadModo;       // 0 = global, 1 = por elemento
    int opacidadGlobal;     // %
    int opacidad[8];        // % por elemento (Elemento)
};

static const Ajustes AJUSTES_FABRICA = {
    1, 1,
    MODO_DINAMICO, MODO_DINAMICO, MODO_DINAMICO, MODO_DINAMICO, MODO_DINAMICO,
    5000, 5000, 5000, 5000, 8000, 3000,
    25,
    1, 1, 1, 1, 1,
    MODO_DINAMICO, 3000,
    1,
    0,
    0, 100,
    { 100, 100, 100, 100, 100, 100, 100, 100 },
};

static Ajustes cfg = AJUSTES_FABRICA;

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

// ---- Rangos a volcar en hexadecimal (para desensamblar fuera de la sesión) ----
static const Rango VOLCADOS[] = {
    { "CRadar::DrawBlips (inicio, la N)",         0x588050, 0x588400 },
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

// Datos del jugador que se está controlando (CWorld::Players[PlayerInFocus]).
static CPlayerInfo& Jugador() {
    return CWorld::Players[static_cast<unsigned char>(CWorld::PlayerInFocus)];
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
//  Punteros de fábrica verificados en dev.2, dev.4, dev.6 y dev.8.
//  Las posiciones de la columna y las estrellas se recalculan en cada
//  frame (ActualizarPosiciones): lo que está oculto deja su hueco y el
//  resto sube; sin icono, la columna se pega a la esquina.
// =====================================================================
// ---- Icono de arma y munición (fábrica X 32, Y 20; munición Y = 20 + 43) ----
static Coordenada iconoX     = { 0x58F927, 0x0085950C, -57.0f, false };  // X = ancho - (esto + parte de WidescreenFix)
static Coordenada iconoY     = { 0x58F913, 0x00858BA4,   4.5f, false };
static Coordenada municionX  = { 0x58FA02, 0x0085950C, -57.0f, false };
static Coordenada municionY  = { 0x58F9DC, 0x00858BA4,   4.5f, false };  // Y base
// ---- Hora (fábrica X 32, Y 22, letra 1,1 / 0,55) ----
static Coordenada horaX      = { 0x58EC16, 0x0085950C,  57.0f, false };
static Coordenada horaY      = { 0x58EC04, 0x00858F8C,  12.0f, false };
static Coordenada horaAlto   = { 0x58EB31, 0x00858F14,  0.64f, false };  // alto de letra
static Coordenada horaAncho  = { 0x58EB47, 0x00866CAC,  0.32f, false };  // ancho de letra
// ---- Dinero (fábrica X 32, Y 89, letra 1,1 / 0,55) ----
static Coordenada dineroX    = { 0x58F5FC, 0x0085950C,  57.0f, false };
static Coordenada dineroY    = { 0x58F5DC, 0x00866C88,  38.5f, false };
static Coordenada dineroAlto = { 0x58F54E, 0x00858F14,  0.64f, false };
static Coordenada dineroAncho= { 0x58F564, 0x00866CAC,  0.32f, false };
// ---- Blindaje y salud (fábrica X 94 / 141 = borde izquierdo, Y 48 / 77) ----
static Coordenada blindajeX  = { 0x58EF59, 0x00866B78, 119.0f, false };
static Coordenada blindajeY  = { 0x58EF3A, 0x00862D38,  43.0f, false };
static Coordenada saludX     = { 0x58EE87, 0x0086535C, 166.0f, false };
static Coordenada saludY     = { 0x58EE68, 0x00866CA8,  60.0f, false };
// ---- Estrellas (fábrica X 29, Y 114; tamaño 1,21 / 0,605) ----
static Coordenada estrellasX = { 0x58DD0F, 0x00863210,   8.0f, false };
static Coordenada estrellasY1= { 0x58DDFC, 0x00866C5C,  72.0f, false };  // encendidas (fld)
static Coordenada estrellasY2= { 0x58DEF5, 0x00866C5C,  72.0f, false };  // intermitentes
static Coordenada estrellasY3= { 0x58DFB3, 0x00866C5C,  72.0f, false };  // fondo de las vacías
static Coordenada estrellaAlto1 = { 0x58DCAA, 0x00866C64, 1.065f, false };
static Coordenada estrellaAncho1= { 0x58DCC0, 0x00866C60, 0.532f, false };
static Coordenada estrellaAlto2 = { 0x58DD70, 0x00866C64, 1.065f, false };
static Coordenada estrellaAncho2= { 0x58DD86, 0x00866C60, 0.532f, false };
static Coordenada estrellaAlto3 = { 0x58DF5D, 0x00866C64, 1.065f, false };
static Coordenada estrellaAncho3= { 0x58DF79, 0x00866C60, 0.532f, false };
// ---- Oxígeno: abajo a la derecha (fábrica X 94 = borde izquierdo, compartido con Blindaje X; largo 62) ----
static Coordenada oxigenoX   = { 0x58F11F, 0x00866B78,  67.0f, false };  // borde derecho a 5 unidades del borde

static Coordenada* const MONTAJE[] = {
    &iconoX, &iconoY, &municionX, &municionY,
    &horaX, &horaY, &horaAlto, &horaAncho,
    &dineroX, &dineroY, &dineroAlto, &dineroAncho,
    &blindajeX, &blindajeY, &saludX, &saludY,
    &estrellasX, &estrellasY1, &estrellasY2, &estrellasY3,
    &estrellaAlto1, &estrellaAncho1, &estrellaAlto2, &estrellaAncho2, &estrellaAlto3, &estrellaAncho3,
    &oxigenoX,
};

// ---- Medidas de la columna (Y "efectiva": sin el desplazamiento por salud máxima) ----
static const float COLUMNA_X_CON_ICONO = 57.0f;   // borde derecho de hora, dinero y barras
static const float COLUMNA_X_SIN_ICONO = 9.0f;    // sin icono: alineada con el borde derecho del icono
static const float COLUMNA_Y_INICIO    = 12.0f;   // primera fila
static const float PASO_HORA      = 14.5f;        // de la fila de la hora a la siguiente
static const float PASO_DINERO    = 13.5f;
static const float PASO_BLINDAJE  = 10.0f;
static const float PASO_SALUD     = 10.0f;
static const float LARGO_BLINDAJE = 62.0f;        // la X de la barra es su borde izquierdo
static const float LARGO_SALUD    = 109.0f;       // a salud máxima (el borde derecho no cambia)
static const float ESTRELLAS_BAJO_ICONO = 60.0f;  // con icono, las estrellas no suben más de aquí
static const float OXIGENO_Y      = 541.0f;

// ---- Desplazamiento del juego con salud máxima < 101 (ReSA, DrawPlayerInfo / DrawWanted) ----
// El juego resta estas unidades a la Y; se suman para que no se descoloque.
static const float DESPL_DINERO    = 12.0f;
static const float DESPL_ESTRELLAS = 12.0f;
static const float DESPL_SALUD     = 10.0f;
static const float DESPL_BLINDAJE  = 3.0f;
static const float DESPL_OXIGENO   = 6.0f;

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
//  ALTO DE LA BARRA DE SALUD
//  gta-reversed (Hud.cpp): RenderHealthBar (0x589270) usa 109 × 9. Se
//  busca dentro de la función una instrucción fmul/fld que lea 9 y solo
//  se redirige si aparece una única vez (probado en dev.12: 0x589358).
//  El largo de la salud aparece dos veces y el blindaje se deja de
//  fábrica (62 × 9): el autor dio por bueno ese resultado.
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
    { "salud alto", 0x589270, 0x5893B0, 9.0f, 0, { 0, 0, 8.0f, false } },
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

static void AplicarTamanos() {
    for (Tamano& t : TAMANOS) {
        if (t.c.redirigida) continue;
        BuscarTamano(t);
        if (t.encontrados == 1) Redirigir(t.c);
    }
}

static void QuitarTamanos() {
    for (Tamano& t : TAMANOS) Restaurar(t.c);
}

// =====================================================================
//  BORDE NEGRO DE LAS BARRAS
//  CSprite2d::DrawBarChart (0x728640) calcula cada lado del borde como
//  "resolución × escala × 2" (fild; fmul [escala]; fadd st, st). La
//  escala es de WidescreenFix, así que su dirección no es fija: se lee
//  al aplicar y el float propio vale esa escala × BORDE_FACTOR.
//  DrawBarChart también dibuja otras barras del juego, por eso el cambio
//  solo dura mientras se dibuja el HUD (drawHudEvent before / after).
//  Volcado verificado en dev.11: las 4 instrucciones son D8 0D.
// =====================================================================
static const float BORDE_FACTOR = 0.7f;

struct Borde {
    unsigned int direccion;  // operando del fmul
    unsigned int original;   // puntero leído al aplicar
    float valor;
    bool redirigida;
};

static Borde BORDES[] = {
    { 0x728866, 0, 0.0f, false },  // arriba
    { 0x7288AB, 0, 0.0f, false },  // abajo
    { 0x7288F7, 0, 0.0f, false },  // izquierda
    { 0x728943, 0, 0.0f, false },  // derecha
};

static void AplicarBordes() {
    for (Borde& b : BORDES) {
        if (b.redirigida) continue;
        const unsigned char* op = reinterpret_cast<const unsigned char*>(b.direccion - 2);
        if (op[0] != 0xD8 || op[1] != 0x0D) continue;  // no es fmul [mem]: no se toca
        unsigned int puntero = *reinterpret_cast<const unsigned int*>(b.direccion);
        if (!SePuedeLeer(puntero, 4)) continue;
        float escala = *reinterpret_cast<const float*>(puntero);
        if (!(escala > 0.0f && escala < 0.01f)) continue;  // valor inesperado: no se toca
        b.original = puntero;
        b.valor = escala * BORDE_FACTOR;
        b.redirigida = EscribirPuntero(b.direccion, reinterpret_cast<unsigned int>(&b.valor));
    }
}

static void QuitarBordes() {
    for (Borde& b : BORDES) {
        if (!b.redirigida) continue;
        if (*reinterpret_cast<const unsigned int*>(b.direccion) != reinterpret_cast<unsigned int>(&b.valor)) continue;
        if (EscribirPuntero(b.direccion, b.original)) b.redirigida = false;
    }
}

// =====================================================================
//  OXÍGENO Y
//  Probado en dev.14: 0x58F100 es la Y del oxígeno (ReSA: fmul [0x86503C]
//  en 0x58F0FE). Como el puntero de fábrica no se anotó, se lee al
//  aplicar: solo se redirige si es un fmul [mem] (D8 0D) que lee un
//  float entre 0 y 448.
// =====================================================================
static const unsigned int OXIGENO_Y_DIR = 0x58F100;
static Coordenada oxigenoY = { OXIGENO_Y_DIR, 0, OXIGENO_Y + DESPL_OXIGENO, false };
static float oxigenoYFabrica = -1.0f;   // valor leído (para el log)

// Comprueba la instrucción y rellena el puntero de fábrica. Devuelve true si es válida.
static bool ComprobarOxigenoY() {
    if (!SePuedeLeer(OXIGENO_Y_DIR - 2, 6)) return false;
    const unsigned char* op = reinterpret_cast<const unsigned char*>(OXIGENO_Y_DIR - 2);
    if (op[0] != 0xD8 || op[1] != 0x0D) return false;
    unsigned int puntero = *reinterpret_cast<const unsigned int*>(OXIGENO_Y_DIR);
    if (!SePuedeLeer(puntero, 4)) return false;
    float valor = *reinterpret_cast<const float*>(puntero);
    oxigenoYFabrica = valor;
    if (!(valor > 0.0f && valor < 448.0f)) return false;
    oxigenoY.original = puntero;
    return true;
}

static bool montajeActivo = false;  // montaje compacto aplicado
static bool activoGlobal = true;    // copia de 'activo' (plugin encendido) para el dibujo

static void AplicarMontaje() {
    if (montajeActivo) return;
    for (Coordenada* c : MONTAJE) Redirigir(*c);
    if (!oxigenoY.redirigida && ComprobarOxigenoY()) Redirigir(oxigenoY);
    AplicarContornos();
    AplicarTamanos();
    montajeActivo = true;
}

static void QuitarMontaje() {
    if (!montajeActivo) return;
    for (Coordenada* c : MONTAJE) Restaurar(*c);
    Restaurar(oxigenoY);
    QuitarContornos();
    QuitarTamanos();
    montajeActivo = false;
}

// =====================================================================
//  OCULTAR ELEMENTOS (técnicas probadas en el mod CLEO, ver DIRECCIONES.md)
//  - ret (0xC3) al principio de funciones cdecl: icono, munición, salud
//    y blindaje. Quien llama limpia la pila, así que es seguro.
//  - 5 NOP en lugar de "call CFont::PrintString" del dinero (0x58F607).
//  Antes de escribir se comprueban los bytes originales; si no coinciden
//  (otro ejecutable u otro mod), ese elemento nunca se oculta.
// =====================================================================
struct Parche {
    const char* nombre;
    unsigned int direccion;
    unsigned int tam;
    unsigned char original[5];
    unsigned char parche[5];
    int estado;              // 0 = sin comprobar, 1 = disponible, -1 = no disponible
    bool aplicado;
};

static Parche parcheIcono    = { "icono",    0x58D7D0, 1, { 0xA1 }, { 0xC3 }, 0, false };
static Parche parcheMunicion = { "municion", 0x5893B0, 1, { 0x53 }, { 0xC3 }, 0, false };
static Parche parcheSalud    = { "salud",    0x589270, 1, { 0x83 }, { 0xC3 }, 0, false };
static Parche parcheBlindaje = { "blindaje", 0x5890A0, 1, { 0x83 }, { 0xC3 }, 0, false };
static Parche parcheDinero   = { "dinero",   0x58F607, 5, { 0xE8, 0xF4, 0xB0, 0x18, 0x00 },
                                                          { 0x90, 0x90, 0x90, 0x90, 0x90 }, 0, false };

static Parche* const PARCHES[] = { &parcheIcono, &parcheMunicion, &parcheSalud, &parcheBlindaje, &parcheDinero };

static bool EscribirBytes(unsigned int dir, const unsigned char* bytes, unsigned int tam) {
    DWORD antes;
    void* p = reinterpret_cast<void*>(dir);
    if (!VirtualProtect(p, tam, PAGE_EXECUTE_READWRITE, &antes)) return false;
    memcpy(p, bytes, tam);
    VirtualProtect(p, tam, antes, &antes);
    return true;
}

static void Ocultar(Parche& p) {
    if (p.aplicado) return;
    if (p.estado == 0) {
        bool igual = SePuedeLeer(p.direccion, p.tam) &&
                     memcmp(reinterpret_cast<const void*>(p.direccion), p.original, p.tam) == 0;
        p.estado = igual ? 1 : -1;
    }
    if (p.estado != 1) return;
    if (memcmp(reinterpret_cast<const void*>(p.direccion), p.original, p.tam) != 0) return;
    p.aplicado = EscribirBytes(p.direccion, p.parche, p.tam);
}

static void Mostrar(Parche& p) {
    if (!p.aplicado) return;
    if (memcmp(reinterpret_cast<const void*>(p.direccion), p.parche, p.tam) != 0) return;
    if (EscribirBytes(p.direccion, p.original, p.tam)) p.aplicado = false;
}

static void Poner(Parche& p, bool oculto) {
    if (oculto) Ocultar(p);
    else        Mostrar(p);
}

// ---- Hora y radar: indicadores del juego (CHud::bDrawClock, CHud::bScriptDontDisplayRadar) ----
// Las misiones también los usan. Para que nunca vean el cambio del
// plugin, solo se cambian mientras se dibuja el HUD (drawHudEvent
// before) y se devuelven justo después (after), como el borde de las
// barras. En gameProcessEvent solo se decide si hay que ocultarlos.
static bool ocultarHora = false, ocultarRadar = false;   // decisión de este frame
static bool horaQuitada = false, radarQuitado = false;   // cambiados en el dibujo actual

static void OcultarIndicadoresEnDibujo() {
    if (ocultarHora && CHud::bDrawClock) { CHud::bDrawClock = false; horaQuitada = true; }

    // El mismo indicador oculta el panel de estadísticas (a pie, en lugar
    // del radar) y el aviso de saltar trayecto: si están en pantalla, no se toca.
    CPed* ped = FindPlayerPed();
    bool estadisticas = ped && !FindPlayerVehicle() && CPad::GetPad(0)->GetDisplayVitalStats(ped);
    bool saltarTrayecto = CGameLogic::SkipCanBeActivated();
    if (ocultarRadar && !estadisticas && !saltarTrayecto && !CHud::bScriptDontDisplayRadar) {
        CHud::bScriptDontDisplayRadar = true;
        radarQuitado = true;
    }
}

static void DevolverIndicadores() {
    if (horaQuitada)  { CHud::bDrawClock = true; horaQuitada = false; }
    if (radarQuitado) { CHud::bScriptDontDisplayRadar = false; radarQuitado = false; }
}

// ---- Diferencia de dinero: lo ganado o perdido mientras se ve el dinero ----
static int diferenciaDinero = 0;                 // 0 = no se muestra
static float diferenciaX = 0.0f, diferenciaY = 0.0f;   // unidades del HUD (Y sin desplazamiento)

// =====================================================================
//  POSICIONES DINÁMICAS
//  Se usa siempre el montaje completo, pero lo que no se ve deja su
//  hueco: los de debajo suben y, sin icono, la columna se pega a la
//  esquina. Las estrellas van debajo de la columna (y del icono si se ve).
//  Con salud máxima < 101 el juego resta DESPL_* a la Y: se compensa.
// =====================================================================
static void ActualizarPosiciones(CPed* ped) {
    bool saludMaxBaja = Jugador().m_nMaxHealth < 101;
    float d = saludMaxBaja ? 1.0f : 0.0f;

    bool hayIcono = !parcheIcono.aplicado;
    float colX = hayIcono ? COLUMNA_X_CON_ICONO : COLUMNA_X_SIN_ICONO;
    float y = COLUMNA_Y_INICIO;

    // ---- Hora (se dibuja si el juego la tiene activada y el plugin no la oculta) ----
    horaX.valor = colX;
    horaY.valor = y;
    if (CHud::bDrawClock && !ocultarHora) y += PASO_HORA;

    // ---- Dinero ----
    dineroX.valor = colX;
    dineroY.valor = y + DESPL_DINERO * d;
    if (!parcheDinero.aplicado) y += PASO_DINERO;

    // ---- Diferencia de dinero (la dibuja el plugin, debajo del dinero) ----
    diferenciaX = colX;
    diferenciaY = y;
    if (cfg.diferencia && !parcheDinero.aplicado && diferenciaDinero != 0) y += PASO_DINERO;

    // ---- Blindaje (el juego no lo dibuja con blindaje <= 1) ----
    blindajeX.valor = colX + LARGO_BLINDAJE;
    blindajeY.valor = y + DESPL_BLINDAJE * d;
    if (!parcheBlindaje.aplicado && ped && ped->m_fArmour > 1.0f) y += PASO_BLINDAJE;

    // ---- Salud ----
    saludX.valor = colX + LARGO_SALUD;
    saludY.valor = y + DESPL_SALUD * d;
    if (!parcheSalud.aplicado) y += PASO_SALUD;

    // ---- Estrellas ----
    float yEstrellas = (hayIcono && y < ESTRELLAS_BAJO_ICONO) ? ESTRELLAS_BAJO_ICONO : y;
    estrellasY1.valor = estrellasY2.valor = estrellasY3.valor = yEstrellas + DESPL_ESTRELLAS * d;

    // ---- Oxígeno ----
    oxigenoY.valor = OXIGENO_Y + DESPL_OXIGENO * d;
}

// =====================================================================
//  COMPORTAMIENTO DINÁMICO (mismas reglas que el Modo 2 del mod CLEO)
//  - Dinero: 5 s tras un cambio.
//  - Arma y munición: 5 s tras cambiar ranura, tipo o munición total.
//  - Salud y blindaje: 5 s tras un cambio; fijas con salud < 25.
//  - Radar: en vehículo, con nivel de búsqueda, esprintando, con un
//    objetivo de misión en el radar o con un destino marcado en el mapa;
//    se oculta 8 s después del último motivo.
//  - Hora: solo con la tecla "." (toque corto: 5 s).
//  - Mantener "." 0,8 s: todo el HUD visible en el montaje compacto; al
//    soltar, se oculta a los 3 s.
//  - Al activar el plugin (o cargar partida): todo visible 3 s.
//  Los tiempos usan el reloj del juego (se para en pausa).
// =====================================================================
static unsigned int tDinero = 0, tArma = 0, tBarras = 0, tRadar = 0, tHora = 0;

// Deja cada elemento "a 3 s de ocultarse".
// Deja 't' de forma que el elemento se vea 'resta' ms más (o su duración, si es menor).
static void Quedan(unsigned int& t, unsigned int ahora, int duracion, int resta) {
    int r = resta < duracion ? resta : duracion;
    t = ahora - static_cast<unsigned int>(duracion - r);
}

static void MostrarTodo3s(unsigned int ahora) {
    Quedan(tDinero, ahora, cfg.msDinero, cfg.msTodo);
    Quedan(tArma,   ahora, cfg.msArma,   cfg.msTodo);
    Quedan(tBarras, ahora, cfg.msBarras, cfg.msTodo);
    Quedan(tHora,   ahora, cfg.msHora,   cfg.msTodo);
    Quedan(tRadar,  ahora, cfg.msRadar,  cfg.msTodo);
}

static bool Visible(unsigned int ahora, unsigned int desde, int duracion) {
    return ahora - desde < static_cast<unsigned int>(duracion);
}

// ---- Valores anteriores para detectar cambios ----
struct EstadoArmas {
    unsigned int ranura;          // unsigned int: sin bytes de relleno para memcmp
    unsigned int tipo[13];
    unsigned int municion[13];
};

static bool datosIniciados = false;
static int dineroAntes = 0;
static EstadoArmas armasAntes;
static float saludAntes = 0.0f, blindajeAntes = 0.0f;

static void LeerArmas(CPed* ped, EstadoArmas& e) {
    memset(&e, 0, sizeof(e));
    e.ranura = ped->m_nSelectedWepSlot;
    for (int i = 0; i < 13; i++) {
        e.tipo[i] = static_cast<unsigned int>(ped->m_aWeapons[i].m_eWeaponType);
        e.municion[i] = ped->m_aWeapons[i].m_nAmmoTotal;
    }
}

// =====================================================================
//  OBJETIVOS EN EL RADAR (CRadar::ms_RadarTrace, plugin-sdk: 0xBA86F0, 175)
//  - Destino propio: marcador con el icono RADAR_SPRITE_WAYPOINT (41),
//    el que pone el jugador en el mapa del menú (gta-reversed MenuManager).
//  - Objetivo de misión: marcador sin icono (RADAR_SPRITE_NONE, el punto
//    de color), visible en el radar, de coche, persona, objeto, punto o
//    zona, y no de corto alcance. Los iconos fijos (tiendas, casas,
//    inicio de misiones) tienen icono propio y no cuentan.
// =====================================================================
static int ContarObjetivos(int* destinos) {
    int objetivos = 0;
    *destinos = 0;
    for (unsigned int i = 0; i < MAX_RADAR_TRACES; i++) {
        const tRadarTrace& t = CRadar::ms_RadarTrace[i];
        if (!t.m_bInUse) continue;
        if (t.m_nRadarSprite == RADAR_SPRITE_WAYPOINT) { (*destinos)++; continue; }
        if (t.m_nRadarSprite != RADAR_SPRITE_NONE || t.m_bShortRange) continue;
        bool enRadar = t.m_nBlipDisplay == BLIP_DISPLAY_BLIP_ONLY || t.m_nBlipDisplay == BLIP_DISPLAY_BOTH;
        bool tipo = t.m_nBlipType >= BLIP_CAR && t.m_nBlipType <= BLIP_CONTACTPOINT;
        if (enRadar && tipo) objetivos++;
    }
    return objetivos;
}

// Hay un objetivo o destino marcado ahora mismo (sin espera al desaparecer).
static bool radarPorMarcador = false;

// Detecta los eventos de este frame y reinicia el tiempo de cada elemento.
static void DetectarEventos(CPed* ped, unsigned int ahora) {
    int dinero = Jugador().m_nMoney;
    if (!ped) return;   // cargando: no hay jugador

    EstadoArmas armas;
    LeerArmas(ped, armas);
    float salud = ped->m_fHealth;
    float blindaje = ped->m_fArmour;

    if (!datosIniciados) {
        // ---- Primera lectura: solo se guardan los valores ----
        diferenciaDinero = 0;
        dineroAntes = dinero;
        armasAntes = armas;
        saludAntes = salud;
        blindajeAntes = blindaje;
        datosIniciados = true;
        return;
    }

    // Cuando el dinero se oculta, la diferencia se olvida (no reaparece con ".").
    if (!Visible(ahora, tDinero, cfg.msDinero)) diferenciaDinero = 0;
    if (dinero != dineroAntes) {
        // Se suma mientras el dinero sigue en pantalla.
        diferenciaDinero += dinero - dineroAntes;
        dineroAntes = dinero;
        tDinero = ahora;
    }
    if (memcmp(&armas, &armasAntes, sizeof(armas)) != 0) { armasAntes = armas; tArma = ahora; }
    if (salud != saludAntes || blindaje != blindajeAntes) {
        saludAntes = salud;
        blindajeAntes = blindaje;
        tBarras = ahora;
    }

    // ---- Radar: motivos con espera (se oculta msRadar después del último) ----
    CWanted* buscado = FindPlayerWanted();
    if ((cfg.radarVehiculo && ped->bInVehicle) ||
        (cfg.radarEsprintar && ped->m_nMoveState == PEDMOVE_SPRINT) ||
        (cfg.radarEstrellas && buscado && buscado->m_nWantedLevel > 0)) {
        tRadar = ahora;
    }

    // ---- Radar: objetivo de misión o destino propio, solo mientras existan ----
    int destinos;
    int objetivos = ContarObjetivos(&destinos);
    radarPorMarcador = (cfg.radarObjetivo && objetivos > 0) || (cfg.radarDestino && destinos > 0);
}

// =====================================================================
//  DIAGNÓSTICO
// =====================================================================
static void DiagnosticoRadar(FILE* f);   // definida en FORMA DEL RADAR
static void DiagnosticoOpacidad(FILE* f);   // definida en OPACIDAD DEL HUD

// Opacidad de cada elemento (menú F9 > Opacidad). Apagado (F11) = 100 %.
static int PorcentajeOpacidad(int elemento) {
    if (!activoGlobal) return 100;
    return cfg.opacidadModo == 0 ? cfg.opacidadGlobal : cfg.opacidad[elemento];
}

static unsigned char Alfa(int elemento, unsigned char a) {
    return static_cast<unsigned char>(a * PorcentajeOpacidad(elemento) / 100);
}
static const unsigned int RADAR_LIMITE_PUBLICO = 0x5832F0;   // CRadar::LimitRadarPoint

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

    // ---- Oxígeno Y ----
    fprintf(f, "\nOxigeno Y (0x%06X)\n\n", OXIGENO_Y_DIR);
    if (oxigenoY.redirigida) {
        fprintf(f, "cambiado a %.1f (fabrica %.1f, puntero 0x%08X)\n", oxigenoY.valor, oxigenoYFabrica, oxigenoY.original);
    } else {
        bool valida = ComprobarOxigenoY();
        const unsigned char* op = reinterpret_cast<const unsigned char*>(OXIGENO_Y_DIR - 2);
        fprintf(f, "bytes %02X %02X  valor %.3f  %s\n", op[0], op[1], oxigenoYFabrica,
                valida ? "valida" : "NO valida (no se toca)");
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

    // ---- Ocultar: estado de cada parche ----
    fprintf(f, "\nParches para ocultar (0 = sin comprobar, 1 = disponible, -1 = no disponible)\n\n");
    for (const Parche* p : PARCHES) {
        fprintf(f, "%-9s 0x%06X  estado %2d  %s  bytes", p->nombre, p->direccion, p->estado,
                p->aplicado ? "oculto " : "visible");
        if (SePuedeLeer(p->direccion, p->tam))
            for (unsigned int i = 0; i < p->tam; i++)
                fprintf(f, " %02X", reinterpret_cast<const unsigned char*>(p->direccion)[i]);
        fprintf(f, "\n");
    }
    fprintf(f, "hora: ocultar %d (bDrawClock %d)\n", ocultarHora, CHud::bDrawClock);
    fprintf(f, "radar: ocultar %d (bScriptDontDisplayRadar %d)\n", ocultarRadar, CHud::bScriptDontDisplayRadar);
    fprintf(f, "salud maxima: %d\n", Jugador().m_nMaxHealth);

    // ---- Marcadores del radar en uso (para ajustar qué cuenta como objetivo) ----
    int destinos;
    int objetivos = ContarObjetivos(&destinos);
    fprintf(f, "\nRadar: objetivos de mision %d, destinos propios %d\n", objetivos, destinos);
    fprintf(f, "Marcadores en uso: indice | icono | tipo | visible | corto alcance | color\n");
    for (unsigned int i = 0; i < MAX_RADAR_TRACES; i++) {
        const tRadarTrace& t = CRadar::ms_RadarTrace[i];
        if (!t.m_bInUse) continue;
        fprintf(f, "  %3u | %2d | %d | %d | %d | %u\n", i, t.m_nRadarSprite, t.m_nBlipType,
                t.m_nBlipDisplay, t.m_bShortRange, t.m_nColour);
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

    // ---- Opacidad: llamadas redirigidas por elemento ----
    DiagnosticoOpacidad(f);

    // ---- Radar: qué módulo tiene enganchadas sus funciones ----
    DiagnosticoRadar(f);

    // ---- Llamadas a CRadar::LimitRadarPoint (E8 rel32) en el código del radar y el HUD ----
    fprintf(f, "\nLlamadas a LimitRadarPoint (0x5832F0) entre 0x580000 y 0x590000:\n");
    if (SePuedeLeer(0x580000, 0x10000)) {
        int total = 0;
        for (unsigned int dir = 0x580000; dir < 0x590000 - 5; dir++) {
            const unsigned char* c = reinterpret_cast<const unsigned char*>(dir);
            if (c[0] != 0xE8) continue;
            if (dir + 5 + *reinterpret_cast<const int*>(c + 1) != RADAR_LIMITE_PUBLICO) continue;
            fprintf(f, "  0x%06X\n", dir);
            total++;
        }
        fprintf(f, "  total: %d\n", total);
    }

    // ---- Volcados en hexadecimal (radar) ----
    for (const Rango& r : VOLCADOS) {
        fprintf(f, "\nVolcado 0x%06X-0x%06X (%s)\n\n", r.inicio, r.fin, r.nombre);
        if (!SePuedeLeer(r.inicio, r.fin - r.inicio)) {
            fprintf(f, "no se puede leer el rango\n");
            continue;
        }
        for (unsigned int dir = r.inicio; dir < r.fin; dir += 16) {
            const unsigned char* b = reinterpret_cast<const unsigned char*>(dir);
            fprintf(f, "%06X:", dir);
            for (int i = 0; i < 16; i++) fprintf(f, " %02X", b[i]);
            fprintf(f, "\n");
        }
    }

    fclose(f);
    return true;
}

// =====================================================================
//  DIBUJO DE LA DIFERENCIA DE DINERO
//  Mismo estilo que el dinero del montaje (Pricedown, letra 0,32 / 0,64,
//  contorno 1, alineado a la derecha). Para que la escala coincida con
//  la del juego (y con WidescreenFix) se leen las escalas que usa el
//  propio dinero: los punteros de 0x58F5F6 (X) y 0x58F5D0 (Y).
//  Solo se dibuja cuando el juego dibuja el HUD (mismas condiciones
//  que CHud::Draw para DrawPlayerInfo, gta-reversed Hud.cpp).
// =====================================================================
static const unsigned int ESCALA_X_DINERO = 0x58F5F6;
static const unsigned int ESCALA_Y_DINERO = 0x58F5D0;

static bool LeerEscala(unsigned int operando, float& escala) {
    if (!SePuedeLeer(operando, 4)) return false;
    unsigned int puntero = *reinterpret_cast<const unsigned int*>(operando);
    if (!SePuedeLeer(puntero, 4)) return false;
    escala = *reinterpret_cast<const float*>(puntero);
    return escala > 0.0f && escala < 0.01f;   // 1/640, 1/448 o la de WidescreenFix
}

static void DibujarDiferencia() {
    if (!cfg.diferencia || !montajeActivo || parcheDinero.aplicado || diferenciaDinero == 0) return;
    if (CReplay::Mode == 1 || CWeapon::ms_bTakePhoto || FrontEndMenuManager.m_bMenuActive) return;
    if (TheCamera.m_bWideScreenOn || !FrontEndMenuManager.m_bPrefsShowHud || !CTheScripts::bDisplayHud) return;

    float ex, ey;
    if (!LeerEscala(ESCALA_X_DINERO, ex) || !LeerEscala(ESCALA_Y_DINERO, ey)) return;
    float sx = SCREEN_WIDTH * ex;    // píxeles por unidad del HUD
    float sy = SCREEN_HEIGHT * ey;

    char texto[24];
    if (diferenciaDinero > 0) sprintf(texto, "+%d", diferenciaDinero);
    else                      sprintf(texto, "-%d", -diferenciaDinero);

    CFont::SetBackground(false, false);
    CFont::SetScale(sx * dineroAncho.valor, sy * dineroAlto.valor);
    CFont::SetProportional(false);
    CFont::SetFontStyle(FONT_PRICEDOWN);
    CFont::SetOrientation(ALIGN_RIGHT);
    CFont::SetRightJustifyWrap(0.0f);
    CFont::SetDropShadowPosition(0);
    CFont::SetEdge(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, Alfa(E_DINERO, 255)));
    CRGBA colorDif = HudColour.GetRGBA(diferenciaDinero > 0 ? HUD_COLOUR_GREEN : HUD_COLOUR_RED);
    colorDif.a = Alfa(E_DINERO, colorDif.a);
    CFont::SetColor(colorDif);
    CFont::PrintString(SCREEN_WIDTH - sx * diferenciaX, sy * diferenciaY, texto);
    CFont::SetEdge(0);
}

// =====================================================================
//  BARRA DE RESISTENCIA (la dibuja el plugin)
//  CPlayerData::m_fTimeCanRun baja al esprintar y sube al andar (gta-
//  reversed, CPlayerPed::HandleSprintEnergy). Se puede esprintar mientras
//  sea mayor de -150; el máximo es CStats::GetFatAndMuscleModifier(
//  STAT_MOD_TIME_CAN_RUN), que depende de la resistencia de CJ.
//  Va donde el oxígeno (abajo a la derecha), con el mismo tamaño y
//  borde; si el oxígeno también se ve, la resistencia va encima.
//  Dinámica: visible mientras no está llena y msResistencia después.
// =====================================================================
static const float RESISTENCIA_MINIMA = -150.0f;
static const float BARRA_LARGO = 62.0f, BARRA_ALTO = 9.0f;   // como el oxígeno
static const float PASO_BARRA_ABAJO = 12.0f;                 // separación sobre el oxígeno
static unsigned int tResistencia = 0;
static float resistencia = 1.0f;       // 0..1
static bool resistenciaInfinita = false;
static bool oxigenoVisible = false;

static void LeerResistencia(CPed* ped, unsigned int ahora) {
    resistenciaInfinita = true;
    oxigenoVisible = false;
    if (!ped || !ped->m_pPlayerData) return;
    CPlayerData* datos = ped->m_pPlayerData;
    resistenciaInfinita = Jugador().m_bDoesNotGetTired || datos->m_bAdrenaline;

    float maximo = CStats::GetFatAndMuscleModifier(STAT_MOD_TIME_CAN_RUN);
    float rango = maximo - RESISTENCIA_MINIMA;
    float r = rango > 0.0f ? (datos->m_fTimeCanRun - RESISTENCIA_MINIMA) / rango : 1.0f;
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    resistencia = r;
    if (r < 0.999f && !resistenciaInfinita) tResistencia = ahora;

    oxigenoVisible = datos->m_fBreath < CStats::GetFatAndMuscleModifier(STAT_MOD_AIR_IN_LUNG);
}

static void DibujarResistencia(unsigned int ahora) {
    if (!activoGlobal || resistenciaInfinita) return;
    if (cfg.modoResistencia == MODO_NUNCA) return;
    if (cfg.modoResistencia == MODO_DINAMICO && !Visible(ahora, tResistencia, cfg.msResistencia)) return;
    if (CReplay::Mode == 1 || CWeapon::ms_bTakePhoto || FrontEndMenuManager.m_bMenuActive) return;
    if (TheCamera.m_bWideScreenOn || !FrontEndMenuManager.m_bPrefsShowHud || !CTheScripts::bDisplayHud) return;

    float ex, ey;
    if (!LeerEscala(ESCALA_X_DINERO, ex) || !LeerEscala(ESCALA_Y_DINERO, ey)) return;
    float sx = SCREEN_WIDTH * ex, sy = SCREEN_HEIGHT * ey;

    // Misma esquina que el oxígeno (borde derecho a 5 unidades del borde de la pantalla).
    float bordeIzq = oxigenoX.valor;                 // X = borde izquierdo
    float arriba = OXIGENO_Y - (oxigenoVisible ? PASO_BARRA_ABAJO : 0.0f);

    if (montajeActivo) AplicarBordes();              // mismo borde fino que el resto
    CSprite2d::DrawBarChart(SCREEN_WIDTH - sx * bordeIzq, sy * arriba,
                            static_cast<unsigned short>(sx * BARRA_LARGO),
                            static_cast<unsigned char>(sy * BARRA_ALTO),
                            resistencia * 100.0f, 0, false, true,
                            CRGBA(225, 190, 60, Alfa(E_RESISTENCIA, 255)), CRGBA(0, 0, 0, 0));
    QuitarBordes();
}

// =====================================================================
//  FORMA DEL RADAR (redondo / cuadrado / rectangular)
//  Desensamblado del volcado de dev.1 (1.0 US, con WidescreenFix):
//  - TransformRadarPointToScreenSpace (0x583480): x = 47·s + 40·s +
//    47·s·p.x. Se redirige solo la constante 40 (operando 0x5834D4) a
//    40 + 47·(ancho − 1): el radar se ensancha hacia la derecha con el
//    mismo zoom (no se deforma). La función no se sustituye: quien la
//    llama espera EDX intacto (gta-reversed).
//  - ClipRadarPoly (0x585040) recorta el mapa a ±1 con valores dentro de
//    las instrucciones: se sustituye entera (jmp) por un recorte propio a
//    ±ancho × ±1. La original ya cambia EDX y ECX, así que es seguro.
//  - LimitRadarPoint (0x5832F0) lleva los iconos lejanos al círculo. En
//    este ejecutable ya la engancha otro mod (empieza con jmp): se
//    encadena. En el mapa del menú se llama a la anterior; en el radar se
//    llevan al borde del rectángulo.
//  - DrawRadarMask (0x585700, empieza con 55 = push ebp): ret, sin
//    máscara redonda.
//  - Aro: 4 llamadas thiscall a CSprite2d::Draw (0x728350) se redirigen
//    a una función vacía que limpia la pila igual (__fastcall, 2 args).
//    El marco rectangular lo dibuja el plugin.
//  Todo se comprueba antes de escribir y se deshace al volver a redondo.
// =====================================================================
static const unsigned int RADAR_TRANSFORMAR = 0x583480;
static const unsigned int OPERANDO_40       = 0x5834D4;   // fmul [40.0]
static const unsigned int OPERANDO_ESCALA_X = 0x5834BC;   // fmul [escala X de WidescreenFix]
static const unsigned int OPERANDO_ESCALA_Y = 0x5834EE;   // fld  [escala Y de WidescreenFix]
static const unsigned int RADAR_RECORTE     = 0x585040;   // CRadar::ClipRadarPoly
static const unsigned int RADAR_LIMITE      = 0x5832F0;   // CRadar::LimitRadarPoint
static const unsigned int RADAR_MASCARA     = 0x585700;   // CRadar::DrawRadarMask
static const unsigned int SPRITE_DRAW       = 0x728350;   // CSprite2d::Draw(CRect const&, CRGBA const&)
static const unsigned int LLAMADAS_ARO[4]   = { 0x58A823, 0x58A8CD, 0x58A977, 0x58AA25 };
static const unsigned int DIBUJANDO_MAPA    = 0xBA67A1;   // FrontEndMenuManager.m_bDrawRadarOrMap
// La "N": DrawBlips la pone a √2 del centro y llama a LimitRadarPoint en
// 0x588161 (volcado de dev.4) antes de DrawRadarSprite(4). En el redondo
// siempre queda fuera del círculo y va al borde; en el rectángulo cabe
// dentro, así que esa llamada se redirige a una función que la lleva
// siempre al borde.
static const unsigned int LLAMADA_NORTE     = 0x588161;

static float radarAncho = 1.0f;          // semiancho en unidades del radar (alto = 1)
static float radarConstante40 = 40.0f;   // valor propio para el operando 0x5834D4
static unsigned int radar40Original = 0;
static bool radarAplicado = false;
static bool radarDibujado = false;       // el mapa del radar se dibujó en este frame

typedef float (__cdecl *FuncLimite)(CVector2D&);
static FuncLimite limiteAnterior = nullptr;      // la del otro mod o un trampolín a la original
static unsigned char limiteBytes[5], recorteBytes[5], aroBytes[4][4], norteBytes[4];
static bool norteAplicado = false;
static unsigned char* trampolin = nullptr;

static bool DibujandoMapa() { return *reinterpret_cast<const unsigned char*>(DIBUJANDO_MAPA) != 0; }

// ---- Recorte de un polígono convexo de 4 vértices a [-ancho, ancho] × [-1, 1] (Sutherland-Hodgman) ----
static int __cdecl RecorteRectangular(CVector2D* salida, const CVector2D* entrada) {
    if (!DibujandoMapa()) radarDibujado = true;
    float limX = DibujandoMapa() ? 1.0f : radarAncho;
    CVector2D a[16], b[16];
    int n = 4;
    for (int i = 0; i < 4; i++) a[i] = entrada[i];
    for (int lado = 0; lado < 4; lado++) {
        int m = 0;
        for (int i = 0; i < n; i++) {
            const CVector2D& p = a[i];
            const CVector2D& q = a[(i + 1) % n];
            float dp, dq;   // distancia con signo al lado (>= 0 = dentro)
            switch (lado) {
                case 0:  dp = limX - p.x; dq = limX - q.x; break;
                case 1:  dp = p.x + limX; dq = q.x + limX; break;
                case 2:  dp = 1.0f - p.y; dq = 1.0f - q.y; break;
                default: dp = p.y + 1.0f; dq = q.y + 1.0f; break;
            }
            if (dp >= 0.0f) b[m++] = p;
            if ((dp >= 0.0f) != (dq >= 0.0f)) {
                float t = dp / (dp - dq);
                b[m++] = CVector2D(p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t);
            }
        }
        n = m;
        for (int i = 0; i < n; i++) a[i] = b[i];
        if (n == 0) break;
    }
    if (n > 8) n = 8;                       // quien llama reserva 8 vértices
    for (int i = 0; i < n; i++) salida[i] = a[i];
    return n;
}

// ---- Iconos lejanos al borde del rectángulo (en el mapa del menú, la anterior) ----
// Devuelve la distancia "rectangular" (<= 1 = dentro del rectángulo). El
// juego solo la usa para decidir si dibuja los iconos de corto alcance
// (tiendas, bares...: solo si <= 1; gta-reversed, DrawEntityBlip /
// DrawCoordBlip). Con la magnitud circular, desaparecían en las esquinas.
static float __cdecl LimiteRectangular(CVector2D& p) {
    if (DibujandoMapa() || !radarAplicado) return limiteAnterior(p);
    float fx = fabsf(p.x) / radarAncho, fy = fabsf(p.y);
    float f = fx > fy ? fx : fy;
    if (f > 1.0f) { p.x /= f; p.y /= f; }
    return f;
}

static void DibujarMarcoRadar();

// ---- La "N": siempre en el borde del rectángulo ----
static float __cdecl LimiteNorte(CVector2D& p) {
    if (DibujandoMapa() || !radarAplicado) return limiteAnterior(p);
    float magnitud = sqrtf(p.x * p.x + p.y * p.y);
    float fx = fabsf(p.x) / radarAncho, fy = fabsf(p.y);
    float f = fx > fy ? fx : fy;
    if (f > 0.0f) { p.x /= f; p.y /= f; }
    return magnitud;
}

// ---- Sustituto de CSprite2d::Draw para el aro (thiscall con 2 argumentos) ----
// En lugar del cuarto de aro, dibuja el marco rectangular. Ocurre dentro de
// CHud::DrawRadar, antes de los iconos, así que el marco queda debajo de
// ellos (como el aro original). Las 4 llamadas dibujan el mismo marco.
static void __fastcall SinAro(void*, void*, const void*, const void*) {
    DibujarMarcoRadar();
}

static void EscribirSalto(unsigned int desde, const void* hasta) {
    unsigned char salto[5];
    salto[0] = 0xE9;
    *reinterpret_cast<int*>(salto + 1) = reinterpret_cast<int>(hasta) - static_cast<int>(desde + 5);
    EscribirBytes(desde, salto, 5);
}

// Prepara 'limiteAnterior' según cómo empiece LimitRadarPoint. Devuelve false si no se reconoce.
static bool PrepararLimite() {
    const unsigned char* f = reinterpret_cast<const unsigned char*>(RADAR_LIMITE);
    memcpy(limiteBytes, f, 5);
    if (f[0] == 0xE9) {                                         // ya enganchada: se encadena
        unsigned int destino = RADAR_LIMITE + 5 + *reinterpret_cast<const int*>(f + 1);
        limiteAnterior = reinterpret_cast<FuncLimite>(destino);
        return true;
    }
    if (f[0] == 0xA0) {                                         // original: mov al, [moffs] (5 bytes)
        if (!trampolin)
            trampolin = static_cast<unsigned char*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!trampolin) return false;
        memcpy(trampolin, f, 5);
        trampolin[5] = 0xE9;
        *reinterpret_cast<int*>(trampolin + 6) = static_cast<int>(RADAR_LIMITE + 5) - reinterpret_cast<int>(trampolin + 10);
        limiteAnterior = reinterpret_cast<FuncLimite>(trampolin);
        return true;
    }
    return false;
}

static bool ComprobarRadar() {
    if (!SePuedeLeer(RADAR_TRANSFORMAR, 0xA0) || !SePuedeLeer(RADAR_RECORTE, 8) || !SePuedeLeer(RADAR_MASCARA, 1)) return false;
    const unsigned char* op40 = reinterpret_cast<const unsigned char*>(OPERANDO_40 - 2);
    if (op40[0] != 0xD8 || op40[1] != 0x0D) return false;
    unsigned int p40 = *reinterpret_cast<const unsigned int*>(OPERANDO_40);
    if (!SePuedeLeer(p40, 4) || *reinterpret_cast<const float*>(p40) != 40.0f) return false;
    static const unsigned char INICIO_RECORTE[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
    if (memcmp(reinterpret_cast<const void*>(RADAR_RECORTE), INICIO_RECORTE, 6) != 0) return false;
    if (*reinterpret_cast<const unsigned char*>(RADAR_MASCARA) != 0x55) return false;
    for (unsigned int llamada : LLAMADAS_ARO) {
        const unsigned char* c = reinterpret_cast<const unsigned char*>(llamada);
        if (c[0] != 0xE8) return false;
        if (llamada + 5 + *reinterpret_cast<const int*>(c + 1) != SPRITE_DRAW) return false;
    }
    radar40Original = p40;
    return true;
}

static int radarEstado = 0;   // 0 = sin comprobar, 1 = se puede, -1 = no se reconoce (no se toca)

static void AplicarFormaRadar(int forma) {
    static const float ANCHOS[] = { 1.0f, 1.0f, 1.5f, 16.0f / 9.0f };
    if (forma == RADAR_REDONDO) {
        if (!radarAplicado) return;
        // ---- Deshacer (solo lo que sigue siendo nuestro) ----
        if (*reinterpret_cast<const unsigned int*>(OPERANDO_40) == reinterpret_cast<unsigned int>(&radarConstante40))
            EscribirPuntero(OPERANDO_40, radar40Original);
        if (*reinterpret_cast<const unsigned char*>(RADAR_RECORTE) == 0xE9) EscribirBytes(RADAR_RECORTE, recorteBytes, 5);
        if (*reinterpret_cast<const unsigned char*>(RADAR_LIMITE) == 0xE9) EscribirBytes(RADAR_LIMITE, limiteBytes, 5);
        if (*reinterpret_cast<const unsigned char*>(RADAR_MASCARA) == 0xC3) EscribirByte(RADAR_MASCARA, 0x55);
        for (int i = 0; i < 4; i++) EscribirBytes(LLAMADAS_ARO[i] + 1, aroBytes[i], 4);
        if (norteAplicado) { EscribirBytes(LLAMADA_NORTE + 1, norteBytes, 4); norteAplicado = false; }
        radarAplicado = false;
        return;
    }

    radarAncho = ANCHOS[forma];
    radarConstante40 = 40.0f + 47.0f * (radarAncho - 1.0f);
    if (radarAplicado) return;

    if (radarEstado == 0) radarEstado = (ComprobarRadar() && PrepararLimite()) ? 1 : -1;
    if (radarEstado != 1) return;

    // ---- Aplicar ----
    memcpy(recorteBytes, reinterpret_cast<const void*>(RADAR_RECORTE), 5);
    for (int i = 0; i < 4; i++) memcpy(aroBytes[i], reinterpret_cast<const void*>(LLAMADAS_ARO[i] + 1), 4);
    EscribirPuntero(OPERANDO_40, reinterpret_cast<unsigned int>(&radarConstante40));
    EscribirSalto(RADAR_RECORTE, reinterpret_cast<const void*>(&RecorteRectangular));
    EscribirSalto(RADAR_LIMITE, reinterpret_cast<const void*>(&LimiteRectangular));
    EscribirByte(RADAR_MASCARA, 0xC3);
    for (unsigned int llamada : LLAMADAS_ARO) {
        int rel = reinterpret_cast<int>(&SinAro) - static_cast<int>(llamada + 5);
        EscribirBytes(llamada + 1, reinterpret_cast<const unsigned char*>(&rel), 4);
    }
    // ---- La "N": solo si la llamada es la esperada (E8 -> LimitRadarPoint) ----
    const unsigned char* n = reinterpret_cast<const unsigned char*>(LLAMADA_NORTE);
    if (SePuedeLeer(LLAMADA_NORTE, 5) && n[0] == 0xE8 &&
        LLAMADA_NORTE + 5 + *reinterpret_cast<const int*>(n + 1) == RADAR_LIMITE) {
        memcpy(norteBytes, n + 1, 4);
        int rel = reinterpret_cast<int>(&LimiteNorte) - static_cast<int>(LLAMADA_NORTE + 5);
        norteAplicado = EscribirBytes(LLAMADA_NORTE + 1, reinterpret_cast<const unsigned char*>(&rel), 4);
    }
    radarAplicado = true;
}

// ---- Marco del radar rectangular (mismas cuentas que TransformRadarPointToScreenSpace) ----
static void DibujarMarcoRadar() {
    if (!radarAplicado) return;

    float ex, ey;
    if (!LeerEscala(OPERANDO_ESCALA_X, ex) || !LeerEscala(OPERANDO_ESCALA_Y, ey)) return;
    float sx = SCREEN_WIDTH * ex, sy = SCREEN_HEIGHT * ey;

    float izquierda = 40.0f * sx;                       // el borde izquierdo no se mueve
    float derecha   = izquierda + 94.0f * sx * radarAncho;
    float arriba    = SCREEN_HEIGHT - 104.0f * sy;
    float abajo     = SCREEN_HEIGHT - 28.0f * sy;
    float g = 2.0f * sy;                                // grosor del marco
    CRGBA negro(0, 0, 0, 255);
    CSprite2d::DrawRect(CRect(izquierda - g, arriba - g, derecha + g, arriba), negro);
    CSprite2d::DrawRect(CRect(izquierda - g, abajo, derecha + g, abajo + g), negro);
    CSprite2d::DrawRect(CRect(izquierda - g, arriba, izquierda, abajo), negro);
    CSprite2d::DrawRect(CRect(derecha, arriba, derecha + g, abajo), negro);
}

static void DiagnosticoRadar(FILE* f) {
    fprintf(f, "\nRadar: modulos de los enganches\n\n");
    {
        struct Enganche { const char* nombre; unsigned int direccion; };
        static const Enganche ENGANCHES[] = {
            { "LimitRadarPoint (0x5832F0)", RADAR_LIMITE },
            { "salto final de DrawRadar (0x58AA2D)", 0x58AA2D },
        };
        for (const Enganche& e : ENGANCHES) {
            const unsigned char* c = reinterpret_cast<const unsigned char*>(e.direccion);
            unsigned int destino = 0;
            if (radarAplicado && e.direccion == RADAR_LIMITE) {
                destino = reinterpret_cast<unsigned int>(limiteAnterior);   // el anterior a nuestro enganche
            } else if (c[0] == 0xE9 || c[0] == 0xE8) {
                destino = e.direccion + 5 + *reinterpret_cast<const int*>(c + 1);
            }
            char modulo[MAX_PATH] = "(ninguno)";
            HMODULE h = nullptr;
            if (destino && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                              reinterpret_cast<LPCSTR>(destino), &h) && h) {
                GetModuleFileNameA(h, modulo, MAX_PATH);
            }
            fprintf(f, "%-38s byte %02X -> 0x%08X  %s\n", e.nombre, c[0], destino, modulo);
            if (destino && SePuedeLeer(destino, 32)) {
                fprintf(f, "  primeros bytes del destino:");
                for (int i = 0; i < 32; i++) fprintf(f, " %02X", reinterpret_cast<const unsigned char*>(destino)[i]);
                fprintf(f, "\n");
            }
        }
    }
}

// =====================================================================
//  OPACIDAD DEL HUD
//  Cada elemento se dibuja dentro de su propia función del juego. Las
//  llamadas de dibujo que hay dentro de cada función (CFont::SetColor /
//  SetDropColor para el texto, CSprite2d::Draw para el icono,
//  CSprite2d::DrawBarChart para las barras) se redirigen a funciones
//  propias que multiplican el alfa y llaman a la original. Solo se cambia
//  una llamada si es E8 hacia la función esperada; F11 lo deshace.
//  Rangos de función: gta-reversed (Hud.cpp) y ReSA (DrawPlayerInfo).
//  El radar no está incluido (su mapa usa un color fijo interno).
// =====================================================================
template <int E> static void __cdecl ColorTexto(CRGBA c)    { c.a = Alfa(E, c.a); CFont::SetColor(c); }
template <int E> static void __cdecl ColorContorno(CRGBA c) { c.a = Alfa(E, c.a); CFont::SetDropColor(c); }

template <int E>
static void __cdecl BarraConAlfa(float x, float y, unsigned short ancho, unsigned char alto, float progreso,
                                 signed char suma, unsigned char porcentaje, unsigned char borde,
                                 CRGBA color, CRGBA colorSuma) {
    color.a = Alfa(E, color.a);
    colorSuma.a = Alfa(E, colorSuma.a);
    CSprite2d::DrawBarChart(x, y, ancho, alto, progreso, suma, porcentaje, borde, color, colorSuma);
}

// ---- Icono de arma: CSprite2d::Draw es thiscall; __fastcall con un argumento falso (EDX) equivale ----
typedef void (__thiscall *DrawRect_t)(CSprite2d*, const CRect&, const CRGBA&);
typedef void (__thiscall *DrawXY_t)(CSprite2d*, float, float, float, float, const CRGBA&);
static void __fastcall IconoRect(CSprite2d* sprite, void*, const CRect& r, const CRGBA& c) {
    CRGBA k = c; k.a = Alfa(E_ARMA, c.a);
    reinterpret_cast<DrawRect_t>(0x728350)(sprite, r, k);
}
static void __fastcall IconoRect2(CSprite2d* sprite, void*, const CRect& r, const CRGBA& c) {
    CRGBA k = c; k.a = Alfa(E_ARMA, c.a);
    reinterpret_cast<DrawRect_t>(0x7283B0)(sprite, r, k);
}
static void __fastcall IconoXY(CSprite2d* sprite, void*, float x, float y, float w, float h, const CRGBA& c) {
    CRGBA k = c; k.a = Alfa(E_ARMA, c.a);
    reinterpret_cast<DrawXY_t>(0x7282C0)(sprite, x, y, w, h, k);
}

// Icono de arma con textura del modelo: se pinta con SetVertices + RwIm2DRenderPrimitive.
typedef void (__cdecl *Vertices_t)(const CRect&, const CRGBA&, const CRGBA&, const CRGBA&, const CRGBA&);
static void __cdecl IconoVertices(const CRect& r, const CRGBA& c1, const CRGBA& c2, const CRGBA& c3, const CRGBA& c4) {
    CRGBA k1 = c1, k2 = c2, k3 = c3, k4 = c4;
    k1.a = Alfa(E_ARMA, c1.a); k2.a = Alfa(E_ARMA, c2.a); k3.a = Alfa(E_ARMA, c3.a); k4.a = Alfa(E_ARMA, c4.a);
    reinterpret_cast<Vertices_t>(0x727420)(r, k1, k2, k3, k4);
}

struct BusquedaDesvio {
    const char* nombre;
    unsigned int inicio, fin;   // rango de la función del elemento
    unsigned int objetivo;      // función original llamada con E8
    const void* nuevo;          // función propia
    int encontrados;
};

static BusquedaDesvio BUSQUEDAS_OPACIDAD[] = {
    { "hora: color",          0x58EAF0, 0x58EC80, 0x719430, reinterpret_cast<const void*>(&ColorTexto<E_HORA>), 0 },
    { "hora: contorno",       0x58EAF0, 0x58EC80, 0x719510, reinterpret_cast<const void*>(&ColorContorno<E_HORA>), 0 },
    { "dinero: color",        0x58F500, 0x58F6A0, 0x719430, reinterpret_cast<const void*>(&ColorTexto<E_DINERO>), 0 },
    { "dinero: contorno",     0x58F500, 0x58F6A0, 0x719510, reinterpret_cast<const void*>(&ColorContorno<E_DINERO>), 0 },
    { "municion: color",      0x5893B0, 0x589650, 0x719430, reinterpret_cast<const void*>(&ColorTexto<E_ARMA>), 0 },
    { "municion: contorno",   0x5893B0, 0x589650, 0x719510, reinterpret_cast<const void*>(&ColorContorno<E_ARMA>), 0 },
    { "estrellas: color",     0x58D9A0, 0x58E020, 0x719430, reinterpret_cast<const void*>(&ColorTexto<E_ESTRELLAS>), 0 },
    { "estrellas: contorno",  0x58D9A0, 0x58E020, 0x719510, reinterpret_cast<const void*>(&ColorContorno<E_ESTRELLAS>), 0 },
    { "icono: Draw (rect)",   0x58D7D0, 0x58D9A0, 0x728350, reinterpret_cast<const void*>(&IconoRect), 0 },
    { "icono: Draw (rect2)", 0x58D7D0, 0x58D9A0, 0x7283B0, reinterpret_cast<const void*>(&IconoRect2), 0 },
    { "icono: Draw (x, y)",   0x58D7D0, 0x58D9A0, 0x7282C0, reinterpret_cast<const void*>(&IconoXY), 0 },
    { "icono: SetVertices",   0x58D7D0, 0x58D9A0, 0x727420, reinterpret_cast<const void*>(&IconoVertices), 0 },
    { "salud: barra",         0x589270, 0x5893B0, 0x728640, reinterpret_cast<const void*>(&BarraConAlfa<E_SALUD>), 0 },
    { "blindaje: barra",      0x5890A0, 0x589190, 0x728640, reinterpret_cast<const void*>(&BarraConAlfa<E_BLINDAJE>), 0 },
    { "oxigeno: barra",       0x589190, 0x589270, 0x728640, reinterpret_cast<const void*>(&BarraConAlfa<E_OXIGENO>), 0 },
};

struct Desvio { unsigned int sitio; unsigned char original[4]; };
static Desvio desvios[128];
static int numDesvios = 0;
static bool opacidadInstalada = false;

static void InstalarOpacidad() {
    if (opacidadInstalada) return;
    numDesvios = 0;
    for (BusquedaDesvio& b : BUSQUEDAS_OPACIDAD) {
        b.encontrados = 0;
        if (!SePuedeLeer(b.inicio, b.fin - b.inicio + 5)) continue;
        for (unsigned int dir = b.inicio; dir < b.fin && numDesvios < 128; dir++) {
            const unsigned char* c = reinterpret_cast<const unsigned char*>(dir);
            if (c[0] != 0xE8) continue;
            if (dir + 5 + *reinterpret_cast<const int*>(c + 1) != b.objetivo) continue;
            Desvio& d = desvios[numDesvios];
            d.sitio = dir;
            memcpy(d.original, c + 1, 4);
            int rel = reinterpret_cast<int>(b.nuevo) - static_cast<int>(dir + 5);
            if (EscribirBytes(dir + 1, reinterpret_cast<const unsigned char*>(&rel), 4)) {
                numDesvios++;
                b.encontrados++;
            }
            dir += 4;
        }
    }
    opacidadInstalada = true;
}

static void QuitarOpacidad() {
    if (!opacidadInstalada) return;
    for (int i = 0; i < numDesvios; i++) EscribirBytes(desvios[i].sitio + 1, desvios[i].original, 4);
    numDesvios = 0;
    opacidadInstalada = false;
}

static void DiagnosticoOpacidad(FILE* f) {
    fprintf(f, "\nOpacidad: llamadas redirigidas (%s)\n\n", opacidadInstalada ? "instalada" : "sin instalar");
    for (const BusquedaDesvio& b : BUSQUEDAS_OPACIDAD)
        fprintf(f, "%-22s 0x%06X-0x%06X -> 0x%06X  encontradas: %d\n", b.nombre, b.inicio, b.fin, b.objetivo, b.encontrados);
}

// =====================================================================
//  MENÚ DE DEPURACIÓN (F9) — temporal, se quitará en la versión final
//  Página principal con subpáginas. Intro entra en una subpágina (o
//  ejecuta "Restaurar"); Retroceso o "< Volver" regresa. Cada opción
//  apunta a un campo de 'cfg'; se guarda al momento en ImmersivedHud.ini.
//  Texto sin acentos: la fuente del juego no los tiene.
// =====================================================================
enum TipoOpcion { OPC_SINO, OPC_MODO, OPC_SEGUNDOS, OPC_PUNTOS, OPC_FORMA, OPC_PORCENTAJE, OPC_OPACIDAD_MODO,
                  OPC_PAGINA, OPC_VOLVER, OPC_ACCION, OPC_TITULO };

struct Opcion {
    const char* texto;
    const char* clave;       // nombre en el .ini
    TipoOpcion tipo;
    int* valor;
    int minimo, maximo, paso;
    int extra;               // OPC_PAGINA: página destino; OPC_PORCENTAJE: 1 = por elemento
};

#define VOLVER { "< Volver", nullptr, OPC_VOLVER, nullptr, 0, 0, 0, 0 }

static const Opcion PAG_PRINCIPAL[] = {
    { "General",                    nullptr, OPC_PAGINA, nullptr, 0, 0, 0, 1 },
    { "Elementos (modo)",           nullptr, OPC_PAGINA, nullptr, 0, 0, 0, 2 },
    { "Tiempos",                    nullptr, OPC_PAGINA, nullptr, 0, 0, 0, 3 },
    { "Radar",                      nullptr, OPC_PAGINA, nullptr, 0, 0, 0, 4 },
    { "Opacidad",                   nullptr, OPC_PAGINA, nullptr, 0, 0, 0, 5 },
    { "",                           nullptr, OPC_TITULO, nullptr, 0, 0, 0, 0 },
    { "Restaurar valores por defecto", nullptr, OPC_ACCION, nullptr, 0, 0, 0, 0 },
};
static const Opcion PAG_GENERAL[] = {
    VOLVER,
    { "Montaje compacto",           "Montaje",        OPC_SINO,     &cfg.montaje, 0, 1, 1, 0 },
    { "Diferencia de dinero",       "Diferencia",     OPC_SINO,     &cfg.diferencia, 0, 1, 1, 0 },
    { "CJ quieto con el menu",      "BloquearCJ",     OPC_SINO,     &cfg.bloquearCJ, 0, 1, 1, 0 },
};
static const Opcion PAG_ELEMENTOS[] = {
    VOLVER,
    { "Dinero",                     "ModoDinero",     OPC_MODO,     &cfg.modoDinero, 0, 2, 1, 0 },
    { "Arma y municion",            "ModoArma",       OPC_MODO,     &cfg.modoArma, 0, 2, 1, 0 },
    { "Salud y blindaje",           "ModoBarras",     OPC_MODO,     &cfg.modoBarras, 0, 2, 1, 0 },
    { "Hora",                       "ModoHora",       OPC_MODO,     &cfg.modoHora, 0, 2, 1, 0 },
    { "Radar",                      "ModoRadar",      OPC_MODO,     &cfg.modoRadar, 0, 2, 1, 0 },
    { "Resistencia",                "ModoResistencia",OPC_MODO,     &cfg.modoResistencia, 0, 2, 1, 0 },
};
static const Opcion PAG_TIEMPOS[] = {
    VOLVER,
    { "Dinero",                     "MsDinero",       OPC_SEGUNDOS, &cfg.msDinero, 500, 60000, 500, 0 },
    { "Arma y municion",            "MsArma",         OPC_SEGUNDOS, &cfg.msArma, 500, 60000, 500, 0 },
    { "Salud y blindaje",           "MsBarras",       OPC_SEGUNDOS, &cfg.msBarras, 500, 60000, 500, 0 },
    { "Hora (tecla .)",             "MsHora",         OPC_SEGUNDOS, &cfg.msHora, 500, 60000, 500, 0 },
    { "Radar tras el ultimo motivo","MsRadar",        OPC_SEGUNDOS, &cfg.msRadar, 0, 60000, 500, 0 },
    { "Resistencia (al llenarse)",  "MsResistencia",  OPC_SEGUNDOS, &cfg.msResistencia, 0, 60000, 500, 0 },
    { "Todo (al soltar . / activar)","MsTodo",        OPC_SEGUNDOS, &cfg.msTodo, 500, 60000, 500, 0 },
    { "Barras fijas con salud <",   "SaludBaja",      OPC_PUNTOS,   &cfg.saludBaja, 0, 200, 5, 0 },
};
static const Opcion PAG_RADAR[] = {
    VOLVER,
    { "Forma del radar",            "FormaRadar",     OPC_FORMA,    &cfg.formaRadar, 0, 3, 1, 0 },
    { "VISIBLE CON",                nullptr,          OPC_TITULO,   nullptr, 0, 0, 0, 0 },
    { "Vehiculo",                   "RadarVehiculo",  OPC_SINO,     &cfg.radarVehiculo, 0, 1, 1, 0 },
    { "Esprintar",                  "RadarEsprintar", OPC_SINO,     &cfg.radarEsprintar, 0, 1, 1, 0 },
    { "Estrellas",                  "RadarEstrellas", OPC_SINO,     &cfg.radarEstrellas, 0, 1, 1, 0 },
    { "Objetivo de mision",         "RadarObjetivo",  OPC_SINO,     &cfg.radarObjetivo, 0, 1, 1, 0 },
    { "Destino propio (mapa)",      "RadarDestino",   OPC_SINO,     &cfg.radarDestino, 0, 1, 1, 0 },
};
static const Opcion PAG_OPACIDAD[] = {
    VOLVER,
    { "Modo",                       "OpacidadModo",   OPC_OPACIDAD_MODO, &cfg.opacidadModo, 0, 1, 1, 0 },
    { "Global (todo el HUD)",       "OpacidadGlobal", OPC_PORCENTAJE, &cfg.opacidadGlobal, 0, 100, 5, 0 },
    { "POR ELEMENTO",               nullptr,          OPC_TITULO,   nullptr, 0, 0, 0, 0 },
    { "Hora",                       "OpacidadHora",       OPC_PORCENTAJE, &cfg.opacidad[E_HORA], 0, 100, 5, 1 },
    { "Dinero",                     "OpacidadDinero",     OPC_PORCENTAJE, &cfg.opacidad[E_DINERO], 0, 100, 5, 1 },
    { "Arma y municion",            "OpacidadArma",       OPC_PORCENTAJE, &cfg.opacidad[E_ARMA], 0, 100, 5, 1 },
    { "Salud",                      "OpacidadSalud",      OPC_PORCENTAJE, &cfg.opacidad[E_SALUD], 0, 100, 5, 1 },
    { "Blindaje",                   "OpacidadBlindaje",   OPC_PORCENTAJE, &cfg.opacidad[E_BLINDAJE], 0, 100, 5, 1 },
    { "Oxigeno",                    "OpacidadOxigeno",    OPC_PORCENTAJE, &cfg.opacidad[E_OXIGENO], 0, 100, 5, 1 },
    { "Resistencia",                "OpacidadResistencia",OPC_PORCENTAJE, &cfg.opacidad[E_RESISTENCIA], 0, 100, 5, 1 },
    { "Estrellas",                  "OpacidadEstrellas",  OPC_PORCENTAJE, &cfg.opacidad[E_ESTRELLAS], 0, 100, 5, 1 },
};

struct Pagina { const char* titulo; const Opcion* opciones; int num; };
#define PAGINA(t, a) { t, a, static_cast<int>(sizeof(a) / sizeof(a[0])) }
static const Pagina PAGINAS[] = {
    PAGINA("Menu",                PAG_PRINCIPAL),
    PAGINA("General",             PAG_GENERAL),
    PAGINA("Elementos (modo)",    PAG_ELEMENTOS),
    PAGINA("Tiempos",             PAG_TIEMPOS),
    PAGINA("Radar",               PAG_RADAR),
    PAGINA("Opacidad",            PAG_OPACIDAD),
};

static bool menuAbierto = false;
static int menuPagina = 0;
static int menuSeleccion = 0;
static int menuSeleccionPrincipal = 0;   // para volver al mismo sitio

// Opción desactivada: las de opacidad por elemento cuando el modo es global.
static bool Desactivada(const Opcion& o) {
    return o.tipo == OPC_PORCENTAJE && o.extra == 1 && cfg.opacidadModo == 0;
}

static void RutaIni(char* ruta) {
    GetModuleFileNameA(NULL, ruta, MAX_PATH);
    char* barra = strrchr(ruta, '\\');
    if (barra) strcpy(barra + 1, "ImmersivedHud.ini");
    else strcpy(ruta, "ImmersivedHud.ini");
}

static void CargarAjustes() {
    char ruta[MAX_PATH];
    RutaIni(ruta);
    for (const Pagina& p : PAGINAS)
        for (int i = 0; i < p.num; i++) {
            const Opcion& o = p.opciones[i];
            if (!o.clave) continue;
            int v = GetPrivateProfileIntA("Ajustes", o.clave, *o.valor, ruta);
            if (v < o.minimo) v = o.minimo;
            if (v > o.maximo) v = o.maximo;
            *o.valor = v;
        }
}

static void GuardarAjustes() {
    char ruta[MAX_PATH], numero[16];
    RutaIni(ruta);
    for (const Pagina& p : PAGINAS)
        for (int i = 0; i < p.num; i++) {
            const Opcion& o = p.opciones[i];
            if (!o.clave) continue;
            sprintf(numero, "%d", *o.valor);
            WritePrivateProfileStringA("Ajustes", o.clave, numero, ruta);
        }
}

static void TextoValor(const Opcion& o, char* texto) {
    static const char* MODOS[] = { "Dinamico", "Siempre", "Nunca" };
    static const char* FORMAS[] = { "Redondo", "Cuadrado", "Rect. 3:2", "Rect. 16:9" };
    texto[0] = '\0';
    switch (o.tipo) {
        case OPC_SINO:          strcpy(texto, *o.valor ? "Si" : "No"); break;
        case OPC_MODO:          strcpy(texto, MODOS[*o.valor]); break;
        case OPC_SEGUNDOS:      sprintf(texto, "%.1f s", *o.valor / 1000.0f); break;
        case OPC_PUNTOS:        sprintf(texto, "%d", *o.valor); break;
        case OPC_FORMA:         strcpy(texto, FORMAS[*o.valor]); break;
        case OPC_PORCENTAJE:    sprintf(texto, "%d %%", *o.valor); break;
        case OPC_OPACIDAD_MODO: strcpy(texto, *o.valor ? "Por elemento" : "Global"); break;
        case OPC_PAGINA:        strcpy(texto, ">"); break;
        default: break;
    }
}

// Pulsación nueva de una tecla (no mantenida). 'antes' guarda el estado anterior.
static bool Pulsada(int tecla, bool& antes) {
    bool ahora = (GetAsyncKeyState(tecla) & 0x8000) != 0;
    bool nueva = ahora && !antes;
    antes = ahora;
    return nueva;
}

static void MoverSeleccion(int paso) {
    const Pagina& p = PAGINAS[menuPagina];
    for (int i = 0; i < p.num; i++) {
        menuSeleccion = (menuSeleccion + paso + p.num) % p.num;
        const Opcion& o = p.opciones[menuSeleccion];
        if (o.tipo != OPC_TITULO && !Desactivada(o)) return;
    }
}

static void IrAPagina(int pagina) {
    if (menuPagina == 0) menuSeleccionPrincipal = menuSeleccion;
    menuPagina = pagina;
    menuSeleccion = (pagina == 0) ? menuSeleccionPrincipal : 0;
}

// ---- CJ quieto con el menú abierto ----
// CPad::DisablePlayerControls (plugin-sdk, CPad + 0x10E): si no es 0, el
// juego ignora los controles del jugador (gta-reversed, Pad.cpp). Las
// misiones lo usan con sus propios valores; el plugin solo añade y quita
// un bit alto propio, y lo reafirma cada frame (una carga lo pone a 0).
static const unsigned short BIT_MENU = 0x8000;

static void BloquearControles(bool bloquear) {
    CPad* pad = CPad::GetPad(0);
    if (!pad) return;
    if (bloquear) pad->DisablePlayerControls |= BIT_MENU;
    else          pad->DisablePlayerControls &= static_cast<unsigned short>(~BIT_MENU);
}

static void ProcesarMenu() {
    static bool f9 = false, arriba = false, abajo = false, izq = false, der = false, intro = false, atras = false;
    if (Pulsada(TECLA_MENU, f9)) {
        menuAbierto = !menuAbierto;
        if (menuAbierto) { menuPagina = 0; menuSeleccion = menuSeleccionPrincipal; }
    }
    BloquearControles(menuAbierto && cfg.bloquearCJ);
    bool pArriba = Pulsada(VK_UP, arriba), pAbajo = Pulsada(VK_DOWN, abajo);
    bool pIzq = Pulsada(VK_LEFT, izq), pDer = Pulsada(VK_RIGHT, der);
    bool pIntro = Pulsada(VK_RETURN, intro), pAtras = Pulsada(VK_BACK, atras);
    if (!menuAbierto) return;

    if (pArriba) MoverSeleccion(-1);
    if (pAbajo)  MoverSeleccion(+1);

    const Opcion& o = PAGINAS[menuPagina].opciones[menuSeleccion];
    if (pAtras && menuPagina != 0) { IrAPagina(0); return; }

    if (pIntro) {
        if (o.tipo == OPC_PAGINA) { IrAPagina(o.extra); return; }
        if (o.tipo == OPC_VOLVER) { IrAPagina(0); return; }
        if (o.tipo == OPC_ACCION) { cfg = AJUSTES_FABRICA; GuardarAjustes(); return; }
    }

    int cambio = pIzq ? -1 : (pDer ? +1 : 0);
    if (cambio == 0 || !o.valor || Desactivada(o)) return;
    int v = *o.valor + cambio * o.paso;
    if (o.tipo == OPC_SINO || o.tipo == OPC_MODO || o.tipo == OPC_FORMA || o.tipo == OPC_OPACIDAD_MODO) {
        if (v < o.minimo) v = o.maximo;           // da la vuelta
        if (v > o.maximo) v = o.minimo;
    } else {
        if (v < o.minimo) v = o.minimo;
        if (v > o.maximo) v = o.maximo;
    }
    *o.valor = v;
    GuardarAjustes();
}

static void DibujarMenu() {
    if (!menuAbierto) return;
    const Pagina& pag = PAGINAS[menuPagina];
    float esc = SCREEN_HEIGHT / 448.0f;                  // unidades de 640 × 448 (sin WidescreenFix)
    float x = 20.0f * esc, ancho = 230.0f * esc;
    float y = 75.0f * esc;                               // justo debajo del cuadro de ayuda
    float alto = 11.0f * esc;                            // alto de cada línea

    // ---- Cuántas líneas caben; si no caben todas, la lista se desplaza ----
    static int primera = 0, paginaAntes = -1;
    if (paginaAntes != menuPagina) { primera = 0; paginaAntes = menuPagina; }
    int caben = static_cast<int>((SCREEN_HEIGHT - y - 30.0f * esc) / alto);
    if (caben < 5) caben = 5;
    if (caben > pag.num) caben = pag.num;
    if (menuSeleccion < primera + 1) primera = menuSeleccion - 1;
    if (menuSeleccion > primera + caben - 1) primera = menuSeleccion - caben + 1;
    if (primera < 0) primera = 0;
    if (primera > pag.num - caben) primera = pag.num - caben;

    CSprite2d::DrawRect(CRect(x - 6.0f * esc, y - 24.0f * esc, x + ancho, y + alto * caben + 18.0f * esc),
                        CRGBA(0, 0, 0, 190));

    CFont::SetBackground(false, false);
    CFont::SetProportional(true);
    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetDropShadowPosition(0);
    CFont::SetEdge(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));
    CFont::SetWrapx(SCREEN_WIDTH);
    CFont::SetRightJustifyWrap(0.0f);

    // ---- Título: versión y página ----
    char titulo[96];
    if (menuPagina == 0) sprintf(titulo, "Immersived HUD %s - F9 cerrar", VERSION);
    else                 sprintf(titulo, "Immersived HUD - %s", pag.titulo);
    CFont::SetScale(0.30f * esc, 0.75f * esc);
    CFont::SetOrientation(ALIGN_LEFT);
    CFont::SetColor(CRGBA(255, 200, 80, 255));
    CFont::PrintString(x, y - 20.0f * esc, titulo);

    CFont::SetScale(0.25f * esc, 0.60f * esc);
    for (int n = 0; n < caben; n++) {
        int i = primera + n;
        const Opcion& o = pag.opciones[i];
        float fy = y + alto * n;
        bool sel = (i == menuSeleccion);
        CRGBA color = o.tipo == OPC_TITULO ? CRGBA(140, 170, 230, 255)
                    : Desactivada(o) ? CRGBA(90, 90, 90, 255)
                    : sel ? CRGBA(255, 255, 255, 255) : CRGBA(180, 180, 180, 255);
        CFont::SetColor(color);
        CFont::SetOrientation(ALIGN_LEFT);
        char linea[96];
        sprintf(linea, "%s%s", sel ? "> " : "  ", o.texto);
        CFont::PrintString(x, fy, linea);

        char valor[32];
        TextoValor(o, valor);
        if (valor[0]) {
            CFont::SetOrientation(ALIGN_RIGHT);
            CFont::PrintString(x + ancho - 10.0f * esc, fy, valor);
        }
    }

    // ---- Pie: ayuda y aviso de que hay más opciones arriba o abajo ----
    char pie[128];
    sprintf(pie, "%s%s%s",
            menuPagina == 0 ? "Flechas: elegir   Intro: entrar" : "Flechas: elegir / cambiar   Retroceso: volver",
            primera > 0 ? "   (mas arriba)" : "", primera + caben < pag.num ? "   (mas abajo)" : "");
    CFont::SetOrientation(ALIGN_LEFT);
    CFont::SetColor(CRGBA(150, 150, 150, 255));
    CFont::PrintString(x, y + alto * caben + 4.0f * esc, pie);
    CFont::SetEdge(0);
}

// =====================================================================
//  PLUGIN
// =====================================================================
static bool activo = true;      // el plugin empieza encendido

// Oculto según el modo: dinámico = oculto si no hay motivo; siempre = visible; nunca = oculto.
static bool Oculto(int modo, bool hayMotivo) {
    if (modo == MODO_SIEMPRE) return false;
    if (modo == MODO_NUNCA) return true;
    return !hayMotivo;
}
static bool reinicio = false;   // partida cargada o nueva (Events::reInitGameEvent)

// Deja el juego como de fábrica: posiciones, tamaños y nada oculto.
static void TodoDeFabrica() {
    QuitarMontaje();
    for (Parche* p : PARCHES) Mostrar(*p);
    ocultarHora = ocultarRadar = false;
}

class ImmersivedHud {
public:
    ImmersivedHud() {
        CargarAjustes();

        // Se ejecuta una vez por frame mientras se juega.
        Events::gameProcessEvent += [] {
            static bool primerFrame = true;
            static bool teclaAntes = false;      // F11 pulsada en el frame anterior
            static bool diagAntes = false;       // F10 pulsada en el frame anterior
            static bool puntoAntes = false;      // "." pulsada en el frame anterior
            static bool puntoLargo = false;      // "." lleva pulsada 0,8 s o más
            static unsigned int tPunto = 0;      // momento en que se pulsó "."
            static unsigned int ahoraAntes = 0;
            static char texto[96];

            unsigned int ahora = CTimer::m_snTimeInMilliseconds;
            CPed* ped = FindPlayerPed();

            // ---- Inicio o partida cargada: evento del juego o salto del reloj ----
            // (el reloj avanza como mucho 300 ms por frame: un salto mayor es una carga)
            if (primerFrame || reinicio || ahora < ahoraAntes || ahora - ahoraAntes > 1000) {
                primerFrame = false;
                reinicio = false;
                datosIniciados = false;
                MostrarTodo3s(ahora);
            }
            ahoraAntes = ahora;

            // ---- Tecla F11: cambia de estado solo al pulsar (no al mantener) ----
            bool teclaAhora = (GetAsyncKeyState(TECLA_ACTIVAR) & 0x8000) != 0;
            if (teclaAhora && !teclaAntes) {
                activo = !activo;
                if (activo) MostrarTodo3s(ahora);

                // ---- Aviso en pantalla (sin acentos: la fuente del juego no los tiene) ----
                sprintf(texto, "Immersived HUD v%s: %s", VERSION, activo ? "activado" : "desactivado");
                CHud::SetHelpMessage(texto, true, false, false);
            }
            teclaAntes = teclaAhora;

            // ---- Tecla F9: menú de depuración ----
            ProcesarMenu();

            // ---- Tecla F10: diagnóstico (solo lectura) ----
            bool diagAhora = (GetAsyncKeyState(TECLA_DIAGNOSTICO) & 0x8000) != 0;
            if (diagAhora && !diagAntes) {
                bool ok = EscribirDiagnostico();
                sprintf(texto, ok ? "Diagnostico guardado en ImmersivedHud.log"
                                  : "No se pudo escribir ImmersivedHud.log");
                CHud::SetHelpMessage(texto, true, false, false);
            }
            diagAntes = diagAhora;

            // ---- Eventos del juego (siempre, para no detectar cambios viejos al encender) ----
            DetectarEventos(ped, ahora);
            LeerResistencia(ped, ahora);
            activoGlobal = activo;

            // ---- Forma del radar (menú F9); apagado = redondo de fábrica ----
            AplicarFormaRadar(activo ? cfg.formaRadar : RADAR_REDONDO);

            // ---- Opacidad (menú F9); apagado = sin desvíos ----
            if (activo) InstalarOpacidad();
            else        QuitarOpacidad();

            if (!activo) {
                puntoAntes = puntoLargo = false;
                TodoDeFabrica();
                return;
            }

            // ---- Tecla ".": toque = hora 5 s; mantener 0,8 s = todo visible ----
            bool puntoAhora = (GetAsyncKeyState(TECLA_CONSULTA) & 0x8000) != 0;
            if (puntoAhora) {
                if (!puntoAntes) tPunto = ahora;
                if (ahora - tPunto >= PULSACION_LARGA) puntoLargo = true;
            } else if (puntoAntes) {
                if (puntoLargo) MostrarTodo3s(ahora);   // todo 3 s, la hora incluida
                else            tHora = ahora;          // la hora msHora
                puntoLargo = false;
            }
            puntoAntes = puntoAhora;

            // Mantener: todo visible en el montaje compacto (cada frame "a 3 s de ocultarse")
            if (puntoLargo) MostrarTodo3s(ahora);

            // ---- Estado deseado de cada elemento (modo del menú F9) ----
            bool saludBaja = ped && ped->m_fHealth < static_cast<float>(cfg.saludBaja);
            bool ocultarDinero = Oculto(cfg.modoDinero, Visible(ahora, tDinero, cfg.msDinero));
            bool ocultarArma   = Oculto(cfg.modoArma,   Visible(ahora, tArma, cfg.msArma));
            bool ocultarBarras = Oculto(cfg.modoBarras, Visible(ahora, tBarras, cfg.msBarras) || saludBaja);

            if (cfg.montaje) AplicarMontaje();
            else             QuitarMontaje();
            Poner(parcheDinero, ocultarDinero);
            Poner(parcheIcono, ocultarArma);
            Poner(parcheMunicion, ocultarArma);
            Poner(parcheSalud, ocultarBarras);
            Poner(parcheBlindaje, ocultarBarras);
            ocultarHora  = Oculto(cfg.modoHora,  Visible(ahora, tHora, cfg.msHora));
            ocultarRadar = Oculto(cfg.modoRadar, Visible(ahora, tRadar, cfg.msRadar) || radarPorMarcador);

            ActualizarPosiciones(ped);
        };

        // ---- Partida cargada o nueva: todo visible 3 s ----
        Events::reInitGameEvent += [] {
            reinicio = true;
        };

        // ---- Solo mientras se dibuja el HUD: borde de las barras, hora y radar ----
        Events::drawHudEvent.before += [] {
            if (montajeActivo) AplicarBordes();
            OcultarIndicadoresEnDibujo();
        };
        Events::drawHudEvent.after += [] {
            QuitarBordes();
            DevolverIndicadores();
            DibujarDiferencia();
            DibujarResistencia(CTimer::m_snTimeInMilliseconds);
            DibujarMenu();
        };
    }
} immersivedHud;
