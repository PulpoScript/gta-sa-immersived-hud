# Registro de cambios

Formato basado en [Keep a Changelog](https://keepachangelog.com/es-ES/1.1.0/). Aún no hay releases publicadas.

## [Sin publicar]

### 0.2.0-dev.11 — Volcado del dibujo del icono de arma (diagnóstico)

- F10 vuelca `CHud::DrawWeaponIcon` (`0x58D7D0-0x58D9A0`) y la zona de `DrawPlayerInfo` que la llama (`0x58F880-0x58F960`), para ver cómo se dibujan los iconos con textura y de dónde sale su alfa. Se quita el volcado de `DrawBlips` (la "N" ya está resuelta).

### 0.2.0-dev.10 — Opacidad del icono de arma por su alfa de llamada (probado)

- ❌ El operando `0x58F8EA` se redirige bien (alfa 63.8 al 25 %), pero el icono con textura (aerosol) sigue opaco. Las llamadas `E8` de `DrawWeaponIcon` son `0x743C60` (`GetWeaponInfo`), `0x53CF70` (`AppendStringToKey`), `0x734E50` (`RwTexDictionaryFindHashNamedTexture`), `0x821B40`, `0x7170C0` y `0x728350` (`CSprite2d::Draw`, el puño); falta saber qué dibuja la textura.

- El log de dev.9 mostró que `DrawWeaponIcon` no llama con `E8` a ninguna función de dibujo reconocible para los iconos con textura (solo el puño, con `CSprite2d::Draw`). En su lugar se cambia el alfa que recibe la función: `DrawPlayerInfo` lo carga con `fld [0x859AAC]` (= 255.0) en `0x58F8EA`; ese operando apunta ahora a un float propio (255 × opacidad). Sirve para el puño y para todas las armas. Se comprueba `D9 05` y el valor 255.0 antes de cambiarlo; F11 lo deshace.
- Se quitan los desvíos del icono de dev.7–dev.9 (evita aplicar la opacidad dos veces al puño).

### 0.2.0-dev.9 — Opacidad de los iconos de arma con textura (probado)

- ❌ El icono con textura no cambiaba: `SetVertices` (`0x727710`) no se usa en `DrawWeaponIcon` (0 llamadas).

- Los iconos de arma (salvo el puño) no cambiaban de opacidad: según gta-reversed (`CHud::DrawWeaponIcon`), se dibujan con `CSprite2d::SetVertices` con coordenadas de textura (`0x727710`) y `RwIm2DRenderPrimitive`; el puño usa `CSprite2d::Draw`. Esa llamada se redirige también, activando la mezcla de alfa si hace falta.
- F10 lista todas las llamadas `E8` de `DrawWeaponIcon`, por si el icono se dibujara de otra forma.

### 0.2.0-dev.8 — Opacidad del radar y del color del dinero (probado)

- ✅ El radar cambia de opacidad.
- ❌ Los iconos de arma con textura (todos menos el puño) no cambiaban.

- **Radar** en F9 > Opacidad (también lo cubre el modo Global). Solo afecta al radar del HUD; el mapa del menú de pausa no cambia. Se redirigen las llamadas de dibujo de: mapa (`DrawRadarSection`, `SetVertices` `0x727890`; se activa la mezcla de alfa porque el juego dibuja el mapa sin ella), zonas de bandas (`DrawAreaOnRadar`), iconos (`DrawRadarSprite`), flecha del jugador (`DrawRotatingRadarSprite`), puntos de objetivo (`ShowRadarTrace` / `ShowRadarTraceWithHeight`) y aro o marco rectangular.
- Las 4 llamadas del aro (`0x58A823`, `0x58A8CD`, `0x58A977`, `0x58AA25`) se redirigen ahora siempre a una única función: con radar redondo dibuja el aro con la opacidad del radar; con rectangular, el marco.
- **Dinero**: el log de dev.7 mostró 0 llamadas de color en `0x58F500-0x58F6A0`. La búsqueda empieza ahora tras el último `CFont::PrintString` (`0x71A700`) anterior al del dinero (`0x58F607`).
- F10 lista también la dirección de cada llamada redirigida.

### 0.2.0-dev.7 — Opacidad del HUD y menú F9 con subpáginas (probado)

- ✅ Menú con subpáginas; opacidad de hora, munición, estrellas, icono y barras (F10: llamadas encontradas).
- ❌ Faltaba el radar y el color del dinero (0 llamadas encontradas).

- **Opacidad** (F9 > Opacidad): modo **Global** (un % para todo el HUD) o **Por elemento** (hora, dinero, arma y munición, salud, blindaje, oxígeno, resistencia y estrellas). En modo Global las opciones por elemento se ven en gris y no se pueden elegir. Pasos de 5 %; se guarda en `ImmersivedHud.ini`.
- Funcionamiento: dentro de la función de cada elemento, las llamadas `E8` a `CFont::SetColor` (`0x719430`), `CFont::SetDropColor` (`0x719510`), `CSprite2d::Draw` (`0x728350`, `0x7283B0`, `0x7282C0`), `CSprite2d::SetVertices` (`0x727420`) y `CSprite2d::DrawBarChart` (`0x728640`) se redirigen a funciones propias que multiplican el alfa y llaman a la original. Solo se cambia una llamada si apunta a la función esperada; F11 lo deshace. La diferencia de dinero usa la opacidad del dinero.
- F10 lista cuántas llamadas se redirigieron por elemento.
- El radar no tiene opacidad todavía (su mapa usa un color fijo interno).
- **Menú F9 con subpáginas**: General, Elementos (modo), Tiempos, Radar y Opacidad. Intro entra, Retroceso o "< Volver" regresa. Las claves del `.ini` no cambian.

### 0.2.0-dev.6 — Iconos de corto alcance en todo el rectángulo (sin probar)

- Los iconos de corto alcance (tiendas, bares...) desaparecían dentro del rectángulo pero fuera del círculo de radio 1: el juego solo los dibuja si la distancia que devuelve `LimitRadarPoint` es ≤ 1 (gta-reversed). En el rectángulo, la función devuelve ahora la distancia rectangular (≤ 1 = dentro del rectángulo).

### 0.2.0-dev.5 — La "N" sigue el borde (probado)

- ✅ La "N" recorre el borde del rectángulo.
- ❌ El icono de la copa (corto alcance) desaparecía dentro del rectángulo.

- La "N" se colocaba a √2 del centro: en el redondo siempre queda fuera del círculo y va al borde, pero en el rectángulo cabe dentro y giraba en un círculo. La llamada de `DrawBlips` a `LimitRadarPoint` para la "N" (`0x588161`) se redirige a una función que la lleva siempre al borde del rectángulo. Se comprueba que sea `E8` hacia `0x5832F0` antes de cambiarla y se deshace al volver a redondo.

### 0.2.0-dev.4 — Diagnóstico de la "N" (probado)

- `DrawBlips` sí llama a `LimitRadarPoint` para la "N" (`0x588161`), antes de `DrawRadarSprite(4)`. Hay 7 llamadas a `LimitRadarPoint` entre `0x580000` y `0x590000`: `0x585B96`, `0x586E97`, `0x587221`, `0x587604`, `0x58798F`, `0x588161` y `0x58820D`.
- CLEO+ también engancha una llamada al principio de `DrawBlips` (`0x58810C`).

- F10 vuelca el inicio de `CRadar::DrawBlips` (`0x588050`, donde se dibuja la "N") y lista todas las llamadas a `LimitRadarPoint` entre `0x580000` y `0x590000`, para saber qué iconos pasan por el recorte rectangular.

### 0.2.0-dev.3 — Marco bajo los iconos (probado)

- ✅ El marco ya no tapa la "N".
- F10: `LimitRadarPoint` tenía un enganche desactivado (salta a un trampolín en `0x401EC5`, dentro del ejecutable, que ejecuta la instrucción original). El salto final de `CHud::DrawRadar` va a **CLEO+** (`CLEO+.cleo`), que intercepta el dibujo de los iconos.
- ❌ La "N" sigue moviéndose en círculo: probablemente `DrawBlips` la coloca con un cálculo propio, sin llamar a `LimitRadarPoint`.

- El marco del radar rectangular se dibuja en lugar del aro (dentro de `CHud::DrawRadar`, antes de los iconos): ya no tapa la "N" ni los demás iconos.
- F10 indica qué módulo (DLL / ASI) tiene enganchados `LimitRadarPoint` y el salto final de `CHud::DrawRadar` (que en este juego va a otra DLL en vez de a `CRadar::DrawBlips`), con los primeros bytes de su código.

### 0.2.0-dev.2 — Forma del radar en el menú F9 (probado)

- ✅ Cuadrado, 3:2 y 16:9 se ven bien.
- ❌ La "N" sigue moviéndose en círculo y el marco la tapaba. Probable causa: otro mod dibuja los iconos (`DrawRadar` termina con `jmp 0x6EE8C442`, fuera del ejecutable) y coloca la "N" sin pasar por `LimitRadarPoint`.

- Nueva opción **"Forma del radar"** en el menú F9: Redondo (por defecto, sin cambios), Cuadrado, Rectangular 3:2 y Rectangular 16:9. Se puede cambiar en pleno juego; F11 apagado vuelve al redondo de fábrica.
- El rectángulo se ensancha hacia la derecha con el mismo zoom: el mapa no se deforma y se ve más terreno a los lados. Marco negro propio, sin aro redondo.
- Técnica (desensamblado del volcado de dev.1):
  - `TransformRadarPointToScreenSpace`: se redirige solo la constante 40 (operando `0x5834D4`) a `40 + 47·(ancho − 1)`.
  - `ClipRadarPoly` (`0x585040`): sustituida por un recorte propio a ±ancho × ±1 (Sutherland-Hodgman).
  - `LimitRadarPoint` (`0x5832F0`): ya la engancha otro mod (empieza con `jmp`); se encadena: en el mapa del menú se llama a la anterior y en el radar los iconos van al borde del rectángulo.
  - `DrawRadarMask` (`0x585700`): `ret` (empieza con `55`).
  - Aro: las 4 llamadas a `CSprite2d::Draw` (`0x728350`) en `0x58A823`, `0x58A8CD`, `0x58A977` y `0x58AA25` van a una función vacía compatible (`__fastcall`, limpia 2 argumentos).
- Todo se comprueba antes de escribir; si algo no coincide, el radar se queda redondo.
- Sin cambiar: el aro giratorio de los aviones sigue siendo redondo.

### 0.2.0-dev.1 — Radar rectangular, paso 1: volcado (sin probar)

- F10 vuelca en hexadecimal el código de `CRadar::LimitRadarPoint`, `TransformRadarPointToScreenSpace`, `ClipRadarPoly`, el inicio de `DrawRadarMask` y `CHud::DrawRadar`, para desensamblarlo y localizar las constantes y llamadas exactas antes de escribir nada. Solo lectura; el radar no cambia.
- Plan (investigado en gta-reversed): el mapa de cada sección se transforma dentro de `DrawRadarSection` sin llamar a una función sustituible, así que el rectángulo se hará con el mismo zoom en ambos ejes, más mapa a los lados y un recorte rectangular (`ClipRadarPoly`). `TransformRadarPointToScreenSpace` no puede sustituirse (quien la llama espera `EDX` intacto): solo se redirigirán sus constantes. El aro son 4 llamadas `thiscall` a `CSprite2d::Draw`: se redirigirán a una función vacía compatible.
- Se elegirá en el menú F9 ("Forma del radar"), con Redondo por defecto.

### 0.1.0-dev.22 — El menú F9 cabe en pantalla (sin probar)

- El menú empieza más arriba (justo debajo del cuadro de ayuda) y, si no caben todas las opciones, la lista se desplaza con la selección. El pie indica si hay más opciones arriba o abajo.

### 0.1.0-dev.21 — CJ quieto con el menú abierto (probado)

- ✅ CJ se queda quieto con el menú abierto. ❌ El menú se salía por abajo de la pantalla (corregido en la dev.22).

- Nueva opción del menú F9, **"CJ quieto con el menu"** (activada por defecto): mientras el menú está abierto, CJ no responde a los controles, así que las flechas solo mueven el menú.
- Técnica: bit propio (`0x8000`) en `CPad::DisablePlayerControls` (plugin-sdk, `CPad + 0x10E`), el indicador con el que el juego ignora los controles del jugador. Se pone y se quita sin tocar los valores de las misiones.

### 0.1.0-dev.20 — Barra de resistencia (sin probar)

- Nueva barra de **resistencia** (lo que CJ puede esprintar antes de cansarse), abajo a la derecha, donde el oxígeno, con el mismo tamaño y borde fino. Color dorado. Si el oxígeno también se ve, la resistencia va justo encima.
- Dato: `CPlayerData::m_fTimeCanRun`, de −150 (agotado) al máximo `CStats::GetFatAndMuscleModifier(STAT_MOD_TIME_CAN_RUN)`, que crece con la estadística de resistencia (gta-reversed, `CPlayerPed::HandleSprintEnergy`).
- Dinámica: visible mientras no está llena y 3 s después de llenarse. No se muestra con resistencia infinita (truco o adrenalina).
- Menú F9: modo de la resistencia (dinámico / siempre / nunca) y su tiempo.

### 0.1.0-dev.19 — Menú de depuración y radar sin espera al quitar el destino (sin probar)

- **Corregido:** al quitar el destino del mapa (o terminar el objetivo), el radar se ocultaba 8 s después. Ahora el objetivo de misión y el destino propio cuentan **solo mientras existen**; los demás motivos (vehículo, esprintar, estrellas) mantienen su espera.
- **Menú de depuración (F9)**, temporal hasta la versión final. Flechas arriba / abajo para elegir, izquierda / derecha para cambiar, Intro para restaurar los valores por defecto.
  - General: montaje compacto (sí / no) y diferencia de dinero (sí / no).
  - Modo de cada elemento (dinero, arma, barras, hora, radar): dinámico, siempre o nunca.
  - Tiempos: dinero, arma, barras, hora, radar tras el último motivo y "todo" (al soltar "." o activar), en pasos de 0,5 s.
  - Umbral de salud baja (barras fijas), en pasos de 5 puntos.
  - Motivos del radar: vehículo, esprintar, estrellas, objetivo de misión y destino propio.
- Los ajustes se guardan al momento en `ImmersivedHud.ini`, junto a `gta_sa.exe`, y se cargan al arrancar.

### 0.1.0-dev.18 — Radar con objetivo de misión o destino (sin probar)

- El radar también se ve mientras haya un **objetivo de misión** en el radar (marcador sin icono de coche, persona, objeto, punto o zona, visible en el radar y no de corto alcance) o un **destino propio** marcado en el mapa (icono 41). Se oculta 8 s después, como con los demás motivos.
- Datos: `CRadar::ms_RadarTrace` (`0xBA86F0`, 175 marcadores, plugin-sdk). Los iconos fijos (tiendas, casas, inicio de misiones) no cuentan.
- F10 anota los marcadores en uso (icono, tipo, visibilidad, corto alcance y color) para ajustar la regla si algo no encaja.

### 0.1.0-dev.17 — Diferencia de dinero (sin probar)

- Debajo del dinero se ve lo ganado (`+250`, verde) o perdido (`-540`, rojo), con la misma letra, contorno y alineación que el dinero. Se suma mientras el dinero sigue en pantalla y se olvida cuando se oculta.
- La fila ocupa su sitio en la columna: el blindaje y la salud bajan mientras se ve.
- Lo dibuja el plugin con `CFont` después del HUD (`drawHudEvent.after`), con las escalas del propio dinero (punteros de `0x58F5F6` y `0x58F5D0`, compatibles con WidescreenFix), y solo cuando el juego dibuja el HUD.
- Sin verificar: que la fuente Pricedown tenga el signo `+`.

### 0.1.0-dev.16 — "." muestra todo con la nueva distribución (probado en mundo libre)

- ✅ Primera prueba del autor en mundo libre: todo bien. Falta probar en misiones y otros momentos del juego antes de cerrar la `0.1.0`.

- Mantener "." 0,8 s muestra todo el HUD en el montaje compacto (antes, HUD de fábrica). Al soltar, se oculta a los 3 s. Para el HUD de fábrica, F11.

### 0.1.0-dev.15 — HUD dinámico (probado en parte)

- ✅ Compila y funciona en el juego. El autor prefiere que mantener "." muestre la nueva distribución, no la de fábrica.


- **Comportamiento** (mismas reglas que el Modo 2 del mod CLEO): dinero 5 s tras un cambio; arma y munición 5 s tras un cambio de ranura, tipo o munición; salud y blindaje 5 s tras un cambio y fijas con salud < 25; radar en vehículo, con nivel de búsqueda o esprintando, y se oculta 8 s después; hora solo con ".".
- **Tecla "."**: toque corto, hora 5 s; mantener 0,8 s, todo el HUD en su posición de fábrica; al soltar, todo visible 3 s.
- **Huecos que se cierran**: lo oculto deja su sitio, los de debajo suben y, sin icono, la columna se pega a la esquina. Las estrellas van debajo de la columna (y del icono si se ve).
- **Salud máxima**: el juego sube dinero (12), estrellas (12), salud (10), blindaje (3) y oxígeno (6) si la salud máxima es menor de 101 (ReSA, desensamblado de `DrawPlayerInfo` y `DrawWanted`). Se compensa en cada frame para que nada se mueva al subirla.
- **Ocultar**: parches `ret` y NOP probados en el mod CLEO (`0x58D7D0`, `0x5893B0`, `0x589270`, `0x5890A0`, `0x58F607`), con comprobación de los bytes originales. La hora (`bDrawClock`) y el radar (`bScriptDontDisplayRadar`) solo se cambian mientras se dibuja el HUD, para no interferir con las misiones. El radar no se oculta mientras se ven el panel de estadísticas (a pie) o el aviso de saltar trayecto, que dependen del mismo indicador.
- El plugin **empieza encendido**; F11 lo apaga (HUD de fábrica). Al activarlo, al empezar y al cargar partida (`reInitGameEvent` o salto del reloj), todo se ve 3 s.
- Corregido: la búsqueda del tamaño del blindaje miraba `DrawTripSkip` (`0x58A160`) en vez de `RenderArmorBar` (`0x5890A0`). No llegó a cambiar nada; se quita y el blindaje queda de fábrica (62 × 9), como se aprobó.
- Verificado antes de subir: compilación de sintaxis con clang 20 y mingw g++ contra plugin-sdk, y revisión independiente (API, estados, memoria y posiciones).

### 0.1.0-dev.14 — Oxígeno abajo a la derecha (probado)

- ✅ El oxígeno aparece abajo a la derecha: `0x58F100` es la Y del oxígeno (valor de fábrica sin anotar).

- Oxígeno X (`0x58F11F`) a 67: borde derecho a 5 unidades del borde de la pantalla, como en el primer boceto.
- Oxígeno Y: candidata `0x58F100` (sin verificar, deducida del patrón de salud y blindaje). Solo se redirige si es `fmul [mem]` con un valor entre 0 y 448. F10 muestra el resultado.

### 0.1.0-dev.13 — Borde de las barras más fino y estrellas más pequeñas (probado)

- ✅ El autor da por buenos el borde y el tamaño de las estrellas.
- La barra de oxígeno se queda en su sitio de fábrica, solapada con el icono.

- Borde negro de las barras al 70 %: se redirige la escala de las 4 instrucciones del borde de `DrawBarChart` (`0x728866`, `0x7288AB`, `0x7288F7`, `0x728943`) solo mientras se dibuja el HUD (`drawHudEvent`), porque la función también dibuja otras barras.
- Estrellas al 88 % (1,21 / 0,605 → 1,065 / 0,532 en sus tres pasadas).

### 0.1.0-dev.12 — Barras un 10 % más pequeñas (probado en parte)

- Solo se aplicó el alto de la salud (`0x589358`, 9 → 8). El largo de la salud aparece 2 veces y el blindaje no se encontró en `0x58A160`–`0x58A230`: no se tocaron. El autor da por bueno el resultado.
- Volcado de `DrawBarChart`: cada lado del borde es `fild [resolución]; fmul [escala de WidescreenFix]; fadd st, st` (× 2).

- Blindaje 62 × 9 → 56 × 8; salud 109 × 9 → 98 × 8 (a salud máxima). El borde derecho sigue alineado con hora y dinero.
- Se busca el valor dentro de `RenderArmorBar` (`0x58A160`) y `RenderHealthBar` (`0x589270`) y solo se redirige si aparece una vez. F10 indica el resultado.

### 0.1.0-dev.11 — Volcado de DrawBarChart (probado)

- F10 añade un volcado en hexadecimal de `0x728640`–`0x728A40` para localizar el grosor del borde de las barras.

### 0.1.0-dev.10 — Contorno más fino en la hora y el dinero (probado)

- ✅ Contorno 1 en hora y dinero: se ve bien.

- ✅ Al desactivar, todos los punteros y el contorno vuelven a su valor de fábrica.
- ✅ `push 2; call CFont::SetEdge` aparece una sola vez en la hora (`0x58EB70`) y en el dinero (`0x58F58D`).
- ❌ En `DrawBarChart` el 2,0 del borde no está como constante en memoria.

- Con F11, el contorno de la hora y el dinero pasa de 2 a 1 (`CFont::SetEdge`, entero: no hay valores intermedios). Se busca `push 2; call 0x719590` en el código de cada uno y solo se cambia si aparece una vez.
- F10 indica dónde está ese contorno y busca en `CSprite2d::DrawBarChart` (`0x728640`) el grosor del borde de las barras (2 unidades, compartido con todas las barras del juego). Las barras no se cambian todavía.

### 0.1.0-dev.9 — Dinero más pequeño y grupo más compacto (probado)

- ✅ El autor da por bueno el montaje completo. Pide un contorno negro un poco más fino en hora, dinero y barras.

- Dinero con la misma letra que la hora (`0x58F54E` alto, `0x58F564` ancho: 0,64 / 0,32).
- Dinero, blindaje, salud y estrellas suben para cerrar el espacio liberado; la salud termina a la altura del icono.

### 0.1.0-dev.8 — Icono en la esquina y hora más pequeña (probado)

- ✅ Icono pegado a la esquina; hora con letra al 58 %: el autor da ese tamaño por bueno.
- Encontrados: letra del dinero (`0x58F54E` = 1,1, `0x58F564` = 0,55, mismas constantes que la hora); tamaño del icono 47 × 58 (`0x58D8CB`/`0x58D935` y `0x58D896`/`0x58D94D`).
- Salud y blindaje tienen una segunda pasada (`0x58EED0` = 194, `0x58EEFD` = 141, `0x58EFA1` = 164, `0x58EFCE` = 94): probablemente el segundo jugador. No se toca.
- Estrellas: tamaño 1,21 / 0,605 en `0x58DCAA`/`0x58DCC0`, `0x58DD70`/`0x58DD86` y `0x58DF5D`/`0x58DF79`.

- El grupo se pega a la esquina: icono, munición, hora, dinero, barras y estrellas se desplazan unas 20 unidades a la derecha; icono y hora suben.
- Hora con letra al 58 % (`0x58EB31` alto, `0x58EB47` ancho): prueba de cambio de tamaño.
- F10 busca también en el código del dinero, las barras y el dibujo del icono (punteros de tamaño).
- Objetivo (boceto del autor): texto al ~60 % del tamaño original, barras más largas y finas, estrellas más pequeñas.

### 0.1.0-dev.7 — Montaje completo: munición, estrellas y márgenes (probado)

- ✅ La munición sigue al icono; las estrellas apagadas se alinean con las encendidas; los márgenes son correctos.
- Pendiente: proporciones. En el boceto el texto es más pequeño y el grupo está pegado a la esquina.

- La munición sigue al icono (`0x58FA02`, el "32" de su X).
- Las estrellas apagadas usan la misma Y que las encendidas (`0x58DEF5`, `0x58DFB3`).
- Más separación entre hora, dinero y blindaje; estrellas debajo de la salud.

### 0.1.0-dev.6 — Búsqueda de la posición de la munición (probado)

- Munición: X = ancho − (32 [`0x58FA02`] + parte de WidescreenFix) + 47 × 0,5; Y = 20 [`0x58F9DC`] + 43 [`0x58F9E6`].
- Icono Y: `0x58F913` (→ `0x858BA4` = 20).
- Estrellas: además de `0x58DDFC`, la Y 114 se usa en `0x58DEF5` y `0x58DFB3`. Tamaño 1,21 / 0,605.
- `0x58FA4C`–`0x58FAB1` repite el cálculo de la munición con un desplazamiento de 138: probablemente el segundo jugador. No se toca.
- La escala de WidescreenFix depende de la resolución (a 1920 × 1080, 1 unidad ≈ 1,93 px).

- F10 también busca instrucciones de float entre `0x58F880` y `0x58FAC0` (icono y munición) y entre `0x58DC80` y `0x58E020` (estrellas).

### 0.1.0-dev.5 — Montaje completo fijo (probado en parte)

- ✅ Icono, hora, dinero y salud quedan donde se esperaba. Dinero muy pegado a la hora.
- ❌ La munición no sigue al icono: se queda en su sitio de fábrica, sobre el hueco del blindaje.
- Estrellas: `0x58DDFC` mueve solo las estrellas encendidas; las apagadas usan otra Y (sin encontrar). La X está bien.
- Blindaje: bien colocado, pero dinero, hora y blindaje demasiado juntos.

- Con F11 encendido, el HUD pasa al montaje compacto: icono en la esquina; a su izquierda hora, dinero, blindaje y salud; estrellas debajo. Al apagar, todo vuelve a su sitio.
- Valores de primera aproximación, sacados de las capturas. Todavía no es dinámico.

### 0.1.0-dev.4 — Búsqueda de la posición de la hora (probado)

- Encontrada: Hora Y en `0x58EC04` (→ `0x858F8C` = 22) y Hora X en `0x58EC16` (→ `0x85950C` = 32, compartido con Dinero X y Arma X). `0x58EB31` (1,1) y `0x58EB47` (0,55) son el tamaño de letra.
- F10 también anota en `ImmersivedHud.log` cada instrucción de float (`fadd`, `fmul`, `fsub`, `fsubr`, `fdiv`, `fdivr`, `fld`) entre `0x58EA80` y `0x58EC80`, con su puntero y su valor. Solo lectura.

### 0.1.0-dev.3 — Primer movimiento: el dinero (probado)

- Probado: el dinero se mueve al encender, vuelve a su sitio al apagar y el resto del HUD no cambia.
- Con WidescreenFix a 1920 × 1080, 1 unidad del HUD ≈ 1,93 píxeles en ambos ejes.
- La Y del dinero es su borde de arriba, menos 12 si la salud máxima es menor de 101. La X del blindaje es su borde izquierdo y la Y su borde de arriba.

- Con F11 encendido, el dinero pasa a X = 100, Y = 140 (posición de prueba); al apagar vuelve a la de fábrica.
- Técnica: se redirige el puntero de la instrucción (`0x58F5FC`, `0x58F5DC`) a un float propio. Solo se escribe si el puntero es el de fábrica.

### 0.1.0-dev.2 — Diagnóstico de direcciones (probado)

Resultado en el juego (con WidescreenFix):

| Elemento | Dirección | Puntero de fábrica | Valor |
|---|---|---|---|
| Dinero X / Y | `0x58F5FC` / `0x58F5DC` | `0x85950C` / `0x866C88` | 32 / 89 |
| Salud X / Y | `0x58EE87` / `0x58EE68` | `0x86535C` / `0x866CA8` | 141 / 77 |
| Blindaje X / Y | `0x58EF59` / `0x58EF3A` | `0x866B78` / `0x862D38` | 94 / 48 |
| Oxígeno X | `0x58F11F` | `0x866B78` (compartido con Blindaje X) | 94 |
| Estrellas X / Y | `0x58DD0F` / `0x58DDFC` (`fld`) | `0x863210` / `0x866C5C` | 29 / 114 |
| Arma X (a) | `0x58F927` | `0x85950C` (compartido con Dinero X) | 32 |
| `0x58F9DC` (`fld`) | | `0x858BA4` | 20 (probablemente Y del icono) |

- Todas son `fmul [mem]` (`D8 0D`) salvo las marcadas `fld` (`D9 05`).
- WidescreenFix redirige Arma X (b), Munición X y las escalas de hora y dinero: escala 1/800 × 1/560 en vez de 1/640 × 1/448.
- La posición de la hora sigue sin encontrarse.


- F10 escribe `ImmersivedHud.log` junto a `gta_sa.exe` con los bytes, el puntero y el float de cada dirección candidata a posición del HUD.
- Solo lee la memoria del juego; no cambia nada.
- Siguiente objetivo acordado: montaje compacto arriba a la derecha (icono en la esquina; a su izquierda hora, dinero, blindaje y salud; estrellas debajo) en el que los elementos ocultos dejan su hueco y el resto se recoloca.

### 0.1.0-dev.1 — Proyecto mínimo (probado)

- Proyecto de Visual Studio basado en plugin-sdk (`Plugin_SA`, Release).
- F11 enciende y apaga el plugin y lo anuncia con `CHud::SetHelpMessage`.
- Al compilar, el `.asi` se copia a `GTA_SA_DIR`.
