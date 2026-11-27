# Todo el programa en la RAM alta (sin la contención del ZX8301)

Los primeros 128 KB de RAM ($20000–$3FFFF) los comparte la CPU con el ZX8301: cada acceso espera al vídeo (en sQLux: `zx8301_insn` y
`zx8301_ram`, tanto en la captura de instrucciones como en los datos). A partir de $40000 no hay contención. Lo que sigue es qué estaba
abajo sin que lo supiéramos, cuánto costaba y cómo se ha arreglado.

## Dónde estaba cada cosa (`tools/addr_parse.py`, `-DPROF_ADDR`)

Medido con Minerva 1.98, JS y RAMTOP de 512, 640 y 768 KB (siempre lo mismo):

| | antes | ahora |
| --- | --- | --- |
| Código del programa | $09–$0B…, **arriba** | arriba |
| Datos globales (`sim`, tablas, BSS) | **arriba** (a continuación del código) | arriba |
| **Pila de la tarea** (`_stack`, 32 KB) | **$3D8FE: en la RAM contenida** | $46962: arriba |
| **`malloc`** (teselas del escenario, objetos, tablas del mundo, modelo del avión, listas de colisión, mapas de letras) | **$2BB1C–$33B6C: en la contenida** | arriba |
| `plat_alloc_hi` (vista 3D, panel, rosa) | arriba (ya estaba) | arriba |
| Pila de la interrupción (8 KB) | arriba (es un array global) | arriba |

Es decir: el código y las variables globales ya iban arriba (QDOS carga la tarea, de ~110 KB, en el primer hueco donde cabe, y por debajo de
$40000 solo quedan, se estima, unos 70 KB libres), pero **la pila y todo lo que daba `malloc` salían del montón común por abajo, que es la RAM interna**. El
`plat_alloc_hi` de la vista solo cubría el buffer de vídeo; lo demás que se lee sin parar —las teselas con los vértices y caras que
transforman `gather_tile`, `xform_obj` y `poly_fast` en cada cuadro, y las llamadas, argumentos y variables locales de todas las funciones en la
pila— corría con los estados de espera del vídeo.

## Qué se ha hecho (`src/platform_ql.c`, `src/main_ql.c`, `src/qdos68k.s`)

* **Tapón** (`plat_himem_init`): al arrancar se llena el hueco bajo del montón con bloques que se quedan reservados (tallas de 32 KB hasta 64 bytes,
  hasta que `malloc` devuelve una dirección ≥ $40000); a partir de ahí **cualquier `malloc` del programa, de la biblioteca de C o de `fopen` cae
  arriba**. Son ~77 KB de RAM interna (`$12E80` en la prueba). Se devuelven al salir (`plat_exit`). Si no hay RAM de expansión (el primer
  `malloc` tras el tapón sale bajo o falla) se suelta el tapón y el programa funciona como antes: en un QL de 128 KB todo es lento y no hay remedio.
* **Pila alta**: `_stack` baja de 32 KB a 6 KB (solo hasta `main`), se reserva un bloque de 24 KB arriba y `plat_run` (`plat_run_on_stack`,
  ensamblador) llama al programa real con la pila allí: guarda la pila vieja en la nueva, hace `jsr` y la recupera al volver. No se toca ningún registro salvo A0, A1 y A7.
* `-DNO_HIMEM` (compilar) deja el comportamiento anterior, para medir.
* **Aviso en el menú**: `plat_low_warn` pinta en rojo «AVISO: PROGRAMA EN RAM LENTA (CONTENIDA)» si hay RAM de expansión y el código o los
  datos globales caen por debajo de $40000.

La parte baja queda **vacía para el programa** (el tapón la ocupa): el menú no usa memoria apreciable (dibuja en la pantalla) y la lista de
teselas del escenario (`tdir_off`, `tdir_size`, 20 KB, solo se leen al cargar una tesela) también está arriba, porque con el tapón no hay forma
de pedir memoria baja. Una reserva de datos «fríos» abajo sería posible dejando un bloque del tapón como reserva y repartiéndolo a mano; ahora
mismo sobraría complejidad por 20 KB que no se leen en el bucle.

## Resultado (escena 1 y 0 de `src/prof.c`, 10 cuadros, SPEED 1,0)

| | todo abajo (`NO_HIMEM`) | pila y monton arriba |
| --- | ---: | ---: |
| Escena 1 (final a 5 km), s por 10 cuadros | 4,30 | **3,38** (−21 %) |
| Escena 0 (cabecera), s por 10 cuadros | 7,52 | **6,18** (−18 %) |
| Planificador, vistas por segundo (escena 1, física en la interrupción) | 0,67 | **0,77** (+15 %) |
| Planificador, pasos de física por segundo | 12,50 | 12,50 |

Todo el programa corría un quinto más despacio por esos accesos. Lo medido antes de este cambio (doc/optimizacion.md,
doc/interrupciones.md, doc/horizonte_rosa.md) se hizo con la pila y el montón abajo; todas esas cifras son por tanto pesimistas en esa
proporción para el programa final.

## Sobre el código

Hoy el ejecutable (~80 KB de código y ~26 KB de datos globales en un mismo bloque) no cabe en los ~70 KB que quedan libres bajo $40000, así que
QDOS lo carga arriba; es lo que sale en las pruebas con ambas ROM. Si el ejecutable se hiciera pequeño (p. ej. quitando el panel o el escenario
de Iberia) podría caer abajo; el aviso del menú lo detecta. Para **forzarlo** arriba haría falta que el hueco bajo estuviera lleno **antes** del
`EXEC`: un primer programa residente que llame a `plat_himem_init`, se quede suspendido (`mt_susjb`) y lance el segundo, o equivalente desde
SuperBASIC. No se ha implementado: no hay forma de probar en el emulador un QDOS que cargue por abajo (los dos ROM probados cargan arriba) y
en esa situación el aviso ya lo dice. La alternativa sería hacer el código independiente de la posición (`bsr` y datos relativos a A4), que c68 no genera.

## Qué mirar en un QL real

* `plat_himem_ok` y el aviso del menú: con 512 KB de RAM de expansión no debe salir el aviso.
* Que abrir ficheros y canales (disco o red) siga funcionando con el tapón puesto: QDOS reserva sus propias estructuras en el montón común
  y, con el hueco bajo lleno, las pone arriba. Si faltara memoria arriba, el programa se queda sin ella antes: el tapón se puede achicar
  quitando tallas de la lista (`plat_himem_init`).
* La pila alta es de 24 KB (antes 32 KB bajos): si hubiera desbordamiento, subir el 24576 de `plat_himem_init` y `plat_run`.
