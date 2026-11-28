Aunque en la vida real nunca lo logre, llevo toda mi vida queriendo ser piloto, desde que con 13-14 años alguien me paso una copia de Microsoft Flight Simulator 3.0, me alucino el reportaje de Microhobby donde explicaban como hacer vuelo por instrumentos en el Fighter Pilot.
En el QL hay un par de simuladores, pero o no se pueden distribuir o dan problema de configuracion, ROM, y demas, ademas la falta de la documentacion Original es un problema en este tipo de juegos.

Asi que con la ayuda de la IA, he intentado solucionar todos estos problemas, es un programa que le queda muy grande a un QL original, la primera version me imprimia un frame cada 17 segundos, a base de optimizar y optimizar ahora casi-casi es jugable, aunque lo ideal es usar una Super Gold Card, una QIMSI Gold, Next o algun QL de fantasia de los que hay por ahi.

En cualquier caso, el resultado, un simulador de vuelo, con la peninsula iberica bastante completita, capaz de hacer vuelo VFR e IFR, con condiciones atmosfericas, ATC, varios modelos de avion... y un completo manual de casi 200 hojas donde encontraras listas de comprobacion, todas las cartas de vuelo, plano de rutas, SID, Aproximacion... etc... para hacer la experiencia de volar en un QL lo mas parecido a la realidad posible.


Alea's Flight Simulator (AFS) en pocas palabras:

- Inspirado en el Microsoft Flight Simulator 1.0 de 1982: cabina con instrumentos, vista exterior en 3D y mapa cenital.
- Escrito en C (c68/XTC68) con ensamblador 68000 en las partes criticas.
- Cuatro aviones con fisica propia: planeador, monomotor de 160 HP, bimotor de 2 x 300 HP con helice de paso variable y tren retractil, y reactor ejecutivo.
- Peninsula Iberica y Canarias a escala 1:2: 14 aeropuertos con pistas, calles de rodaje, PAPI y luces, unos 20 VOR/DME y 9 NDB, ciudades, rios, autovias y los principales sistemas montañosos. Los datos son aproximados: no sirven para navegar de verdad.
- Radionavegacion: COM1, NAV1, NAV2, ADF, DME, transpondedor, ILS, VOR con OBS, NDB y balizas.
- Control de transito aereo en ingles: ATIS, torre, autorizaciones y contacto de radar.
- Panel completo en modo 4 (512x256), vistas de cabina, persecucion, torre y lateral, dia, atardecer y noche con luces de pista y ciudad.
- Perdida, flaps, mezcla, fallo de motor, rotura estructural, choques, guardar y cargar partida.
- Sonido con QSound 2+ (opcional).

Requisitos: QL con 640 KB, disquete de 720 KB o disco win1_ (el escenario ocupa 450 KB), o un emulador (sQLux, QPC2, QXL...). Para que vaya fluido hace falta como minimo la velocidad de un QL x8 (Super Gold Card); en un QL a 7,5 MHz funciona, pero muy despacio (el panel y la vista se refrescan una o dos veces por segundo).

Aviso: el programa lo ha escrito una IA, asi que puede haber bugs. No esta basado en la vida real y, por supuesto, no sirve para aprender a pilotar.

Codigo fuente, documentacion y capturas: https://github.com/alvaroalea/ALEA_FS
Descarga (imagen de disquete, disco win1_ y manual en PDF): https://github.com/alvaroalea/ALEA_FS/releases
