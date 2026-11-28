"""cartas_en.py - traduccion al ingles de los textos de las cartas (se usa con QFS_LANG=en, ver cartas.py).
tr(s) recibe cada cadena que se dibuja. Las cabeceras ya bilingues ("DATOS ... / DATA ...") se quedan con la mitad inglesa; el resto pasa
por la lista de frases. Los nombres propios (ciudades, montes, puntos de notificacion) no se tocan."""
import re

SP = re.compile(r"[áéíóúñÁÉÍÓÚÑ]|\b(DE|DEL|ESPERA|CIRCUITO|Rojas|Verdes|Administración|Dirección|Posición|Elevación|Ondulación|Declinación|"
                r"Tipo|Categoría|Horario|Coordenadas|OBSTÁCULO|Información|Frecuencias)\b")
BIL = re.compile(r"^((?:AD [\d./ ]+?  )?)(.+?) / (.+)$")

# (expresion, sustitucion) en orden; las mas largas primero
RULES = [
    (r"CARTA DEL SIMULADOR AFS - NO APTA PARA LA NAVEGACIÓN REAL", "AFS SIMULATOR CHART - NOT FOR REAL NAVIGATION"),
    (r"AFS - Carta de navegación en ruta - Península Ibérica", "AFS - En-route navigation chart - Iberian Peninsula"),
    (r"AFS - Cartas de aeropuerto: ADC, datos AD (\d+), SID e IAC", r"AFS - Aerodrome charts: ADC, AD \1 data, SID and IAC"),
    (r"AFS - Cartas de navegación visual \(VAC\)", "AFS - Visual approach charts (VAC)"),
    (r"AFS - Cartas de aeropuerto \(ADC, SID, STAR/ILS\)", "AFS - Aerodrome charts (ADC, SID, STAR/ILS)"),
    (r"AFS – Simulador de vuelo para Sinclair QL", "AFS – Flight simulator for the Sinclair QL"),
    (r"Escala (\S+) del simulador \((\S+) cm = (\S+) km del simulador\)", r"Simulator scale \1 (\2 cm = \3 km of the simulator)"),
    (r"Distancias a escala (\S+) del simulador\. Rumbos verdaderos\. Elevaciones y altitudes en pies\.",
     r"Distances to simulator scale \1. True headings. Elevations and altitudes in feet."),
    (r"CARTA DE NAVEGACIÓN EN RUTA  ·  PENÍNSULA IBÉRICA, BALEARES Y CANARIAS", "EN-ROUTE NAVIGATION CHART  ·  IBERIAN PENINSULA, BALEARIC AND CANARY ISLANDS"),
    (r"CARTA DE NAVEGACIÓN VISUAL", "VISUAL NAVIGATION CHART"),
    (r"CARTA DE SALIDA NORMALIZADA", "STANDARD INSTRUMENT DEPARTURE CHART"),
    (r"VUELO POR INSTRUMENTOS \(SID\) - OACI", "INSTRUMENT FLIGHT (SID) - ICAO"),
    (r"CARTA DE APROXIMACIÓN", "INSTRUMENT APPROACH CHART"),
    (r"POR INSTRUMENTOS - OACI", "ICAO"),
    (r"PLANO DE AERÓDROMO", "AERODROME CHART"),
    (r"DATOS DEL AERÓDROMO", "AERODROME DATA"),
    (r"ISLAS CANARIAS \(recuadro desplazado: no está en su posición real\)", "CANARY ISLANDS (inset moved: not in its real position)"),
    (r"Aeropuerto con ILS \(OACI, frecuencias ILS y TWR\)", "Airport with ILS (ICAO, ILS and TWR frequencies)"),
    (r"Aerovía: designador, distancia NM, rumbo en cada tramo", "Airway: designator, distance NM, heading on each leg"),
    (r"Altitud mínima de cuadrícula \((\S+)\), (\S+) ft", r"Grid minimum altitude (\1), \2 ft"),
    (r"Relieve del simulador \(conos montañosos\)", "Simulator terrain (mountain cones)"),
    (r"Altitud de transición: (\S+) ft\. Mundo plano a cota (\S+)\.", r"Transition altitude: \1 ft. Flat world at elevation \2."),
    (r"NDB  \(nombre, frecuencia kHz, ident\)", "NDB  (name, frequency kHz, ident)"),
    (r"VOR/DME  \(nombre, frecuencia MHz, ident, Morse\)", "VOR/DME  (name, frequency MHz, ident, Morse)"),
    (r"Llamar en los puntos N, E, S o W", "Call at points N, E, S or W"),
    (r"Punto culminante \(pies\)", "Highest point (feet)"),
    (r"Alcance (\S+) km del simulador", r"Range \1 km of the simulator"),
    (r"Sin relieve en (\S+) NM", r"No terrain within \1 NM"),
    (r"Tintas: ", "Tints: "),
    (r"Cumbre (\S+)", r"Summit \1"),
    (r"Autovía", "Motorway"), (r"^Ciudad$", "City"), (r"^Río$", "River"), (r"^Mar$", "Sea"),
    (r"^AERÓDROMO$", "AERODROME"), (r"^CIRCUITO DE TRÁNSITO$", "TRAFFIC CIRCUIT"), (r"^ALTITUDES$", "ALTITUDES"),
    (r"Elevación (\S+) ft", r"Elevation \1 ft"),
    (r"Altitud (\S+) ft, virajes a izquierdas", r"Altitude \1 ft, left-hand turns"),
    (r"salvo indicación de TWR\.", "unless TWR instructs otherwise."),
    (r"Velocidad máx\. (\S+) KT en el CTR\.", r"Max speed \1 KT in the CTR."),
    (r"Espacio aéreo D: SFC-(\S+) ft", r"Class D airspace: SFC-\1 ft"),
    (r"Por encima: espacio aéreo C\.", "Above: class C airspace."),
    (r"\(posición real aproximada\)", "(approximate real position)"),
    (r"\(real aprox\.\)", "(real, approx.)"),
    (r"\((\S+) m\) \(rumbos verdaderos en todo el simulador\)", r"(\1 m)"),
    (r"(\S+)° \(rumbos verdaderos en todo el simulador\)", r"\1° (true headings throughout the simulator)"),
    (r"No aplica: mundo plano a cota (\S+)", r"Not applicable: flat world at elevation \1"),
    (r"(\S+) km (\S+) de (.+) \(simulador\)", r"\1 km \2 of \3 (simulator)"),
    (r"AFS Aeropuertos · TWR", "AFS Airports · TWR"),
    (r"Cartas: ", "Charts: "),
    (r"Calles de rodaje: (\S+) m de anchura\. Plataforma (\S+) x (\S+) m\. Luces de borde, umbral y extremo de pista\.",
     r"Taxiways: \1 m wide. Apron \2 x \3 m. Edge, threshold and runway end lights."),
    (r"Blancas cada (\S+) m", r"White every \1 m"),
    (r"COORDENADAS \(aprox\.\)", "COORDINATES (approx.)"),
    (r"THR \(coordenadas aprox\.\)", "THR (approx. coordinates)"),
    (r"Frecuencias y posiciones aproximadas\.", "Frequencies and positions are approximate."),
    (r"Información en bucle \(en inglés\)", "Looping information (in English)"),
    (r"Torre y aproximación; tecla K para llamar", "Tower and approach; press K to call"),
    (r"Combustible AVGAS (\S+) y JET A1", r"Fuel AVGAS \1 and JET A1"),
    (r"OBSTÁCULO \(cumbre\)", "OBSTACLE (summit)"),
    (r"^FIJO$", "FIX"), (r"^USO$", "USE"),
    (r"Pedir autorización a TWR \(tecla K\) antes de despegar y al entrar en el CTR \((\S+) NM, SFC-(\S+) ft\)\.",
     r"Request clearance from TWR (key K) before take-off and on entering the CTR (\1 NM, SFC-\2 ft)."),
    (r"Esperas: (\S+) KT máx\., virajes a derechas, tramos de (\S+) MIN\. Velocidad máx\. (\S+) KT bajo FL(\S+)\.",
     r"Holds: \1 KT max, right-hand turns, \2 MIN legs. Max speed \3 KT below FL\4."),
    (r"PENDIENTE MÍNIMA DE ASCENSO (\S+) % \((\S+) ft/NM\) hasta la altitud mínima del fijo de salida\.",
     r"MINIMUM CLIMB GRADIENT \1 % (\2 ft/NM) up to the minimum altitude of the departure fix."),
    (r"Ascenso inicial: (\S+) ft o según autorización de TWR\. Velocidad máx\. (\S+) KT bajo FL(\S+)\.",
     r"Initial climb: \1 ft or as cleared by TWR. Max speed \2 KT below FL\3."),
    (r"Rumbo de pista (\S+) hasta (\S+) ft; sin viraje para interceptar el R-(\S+) (\w+)", r"Runway heading \1 to \2 ft; no turn to intercept R-\3 \4"),
    (r"Rumbo de pista (\S+) hasta (\S+) ft; viraje progresivo a la derecha para interceptar el R-(\S+) (\w+)",
     r"Runway heading \1 to \2 ft; continuous right turn to intercept R-\3 \4"),
    (r"Rumbo de pista (\S+) hasta (\S+) ft; viraje progresivo a la izquierda para interceptar el R-(\S+) (\w+)",
     r"Runway heading \1 to \2 ft; continuous left turn to intercept R-\3 \4"),
    (r"hasta (\w+) \((D\S+ \w+)\)\. Cruzar (\w+) a (\S+) ft o superior\.", r"to \1 (\2). Cross \3 at \4 ft or above."),
    (r"antes de entrar en el CTR\.", "before entering the CTR."),
    (r"FRUSTRADA: ascender en rumbo de pista (\S+) hasta (\S+) ft; viraje progresivo a la derecha directo a (\w+) ascendiendo a (\S+) ft y esperar\.",
     r"MISSED APCH: climb on RWY HDG \1 to \2 ft, continuous right turn direct \3 climbing to \4 ft and hold."),
    (r"FRUSTRADA: ascender en rumbo de pista (\S+) hasta (\S+) ft; viraje progresivo a la izquierda directo a (\w+) ascendiendo a (\S+) ft y esperar\.",
     r"MISSED APCH: climb on RWY HDG \1 to \2 ft, continuous left turn direct \3 climbing to \4 ft and hold."),
    (r"FRUSTRADA: ascender en rumbo de pista (\S+) hasta (\S+) ft; en línea recta directo a (\w+) ascendiendo a (\S+) ft y esperar\.",
     r"MISSED APCH: climb on RWY HDG \1 to \2 ft, straight ahead direct \3 climbing to \4 ft and hold."),
    (r"Elevación del umbral (\S+) ft: OCA = OCH \+ (\S+)\. RVR/VIS según el operador\.", r"Threshold elevation \1 ft: OCA = OCH + \2. RVR/VIS as per operator."),
    (r"Pendiente mínima de frustrada (\S+) %\.", r"Minimum missed approach gradient \1 %."),
    (r" requeridos\.", " required."),
    (r"Llegadas desde los IAF: radar o propia navegación\.", "Arrivals from the IAFs: radar or own navigation."),
    (r" y DME", " and DME"), (r"ILS, VOR y DME", "ILS, VOR and DME"),
    (r"Distancias en NM del simulador\. Altitudes en ft \(alturas entre paréntesis\)\.", "Distances in simulator NM. Altitudes in ft (heights in parentheses)."),
    (r"RDH (\S+) ft · (\S+) m tras el umbral", r"RDH \1 ft · \2 m past the threshold"),
    (r"(\S+) m al umbral", r"\1 m to the threshold"), (r"(\S+) NM al umbral", r"\1 NM to the threshold"),
    (r"(\S+) m \+ barra (\S+) m", r"\1 m + bar \2 m"),
    (r"(\S+) m · pendiente (\S+) %", r"\1 m · slope \2 %"),
    (r"Descenso (\S+)° \((\S+) %\)", r"Descent \1° (\2 %)"),
    (r"(\S+)° IN · (\S+) MIN · DCHA", r"\1° IN · \2 MIN · RIGHT"), (r"(\S+)° IN · (\S+) MIN · IZDA", r"\1° IN · \2 MIN · LEFT"),
    (r"NM del simulador", "simulator NM"),
    (r"^SID \(salida\)$", "SID (departure)"),
    (r"^ESPERA ", "HOLD "), (r"^PLATAFORMA$", "APRON"), (r"^DIMENSIONES$", "DIMENSIONS"), (r"^RUMBO V$", "TRUE BRG"), (r"^AYUDAS$", "AIDS"),
    (r"^PISTA (\S+) · Altitud de transición (\S+) ft · Velocidad máxima (\S+) KT por debajo de FL(\S+)",
     r"RUNWAY \1 · Transition altitude \2 ft · Maximum speed \3 KT below FL\4"),
    (r"\(rumbo (\S+), (\S+) NM\)\. Altitud inicial (\S+) ft, enlace con la aerovía (\w+)\.", r"(heading \1, \2 NM). Initial altitude \3 ft, link to airway \4."),
    (r"desde (\w+) \(aerovía (\S+)\) rumbo (\S+) (\S+) NM a (\w+), cruzar (\w+) a (\S+) ft; después a IF (\w+) a (\S+) ft\.",
     r"from \1 (airway \2) heading \3 \4 NM to \5, cross \6 at \7 ft; then to IF \8 at \9 ft."),
    (r"Llegadas a la pista (\S+) en uso\. Velocidad máxima (\S+) KT por debajo de FL(\S+), (\S+) KT desde el IAF\.",
     r"Arrivals to runway \1 in use. Maximum speed \2 KT below FL\3, \4 KT from the IAF."),
    (r"Horario / hours", "Hours"),
]
RULES = [(re.compile(a), b) for a, b in RULES]
_cache = {}
EXACT = {"Elevación / temp. referencia": "Elevation / reference temp."}


def tr(s):
    r = _cache.get(s)
    if r is not None:
        return r
    t = EXACT.get(s, s)
    m = BIL.match(t) if t == s else None
    if m and SP.search(m.group(2)):
        t = m.group(1) + m.group(3)
    for rx, rep in RULES:
        t = rx.sub(rep.replace("%", "%"), t)
    _cache[s] = t
    return t
