/*
    Descripción general:
    Programa que lee un archivo de bitácora (bitacora.txt) y construye una
    estructura tipo grafo dirigido lógico con la siguiente forma:

        Raíz
          └── Redes (primeros dos octetos de la IP, por ejemplo "119.232")
                └── Hosts (IP completa sin puerto, por ejemplo "119.232.101.246")
                      └── Entradas (fecha, hora, puerto, mensaje de error)

    A partir de esta estructura se calcula:
    - La(s) red(es) con mayor número de hosts únicos (grado de salida de nodos red).
    - El/los host(s) (IP completa sin puerto) con mayor número de entradas
      en la bitácora (grado de salida de nodos host).

    Restricciones:
    - No se usan vector, unordered_map, algorithm, etc.
    - Solo se utilizan: <iostream>, <fstream>, <sstream>, <string>.
    - El archivo se llama exactamente "bitacora.txt" y no se pide al usuario.

    Complejidad general aproximada:
    - Lectura y procesamiento de N líneas: O(N * L), donde L es la longitud
      promedio de la línea (pequeña).
    - Operaciones sobre tablas hash de tamaño TABLE_SIZE: O(1) promedio por inserción/búsqueda.
    - Búsquedas de máximos sobre las tablas: O(TABLE_SIZE).
    - Complejidad total: O(N) en tiempo (para N grande) y O(TABLE_SIZE) en espacio.
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace std;

// -----------------------------------------------------------------------------
// 1. Parámetros globales y estructuras de datos
// -----------------------------------------------------------------------------

/*
 * Tamaño de las tablas hash.
 * Se usa un tamaño grande para reducir colisiones y soportar muchos hosts/redes.
 * Si en algún caso extremo la tabla se llenara, el programa lo indicaría.
 *
 * Espacio: O(TABLE_SIZE) para hosts + O(TABLE_SIZE) para redes.
 */
const int TABLE_SIZE = 1000003;

/*
 * struct Entry
 * Representa una entrada de la bitácora asociada a un host:
 * fecha, hora, puerto y mensaje.
 * En este proyecto no se usa para cálculos de máximos, pero respeta
 * la idea de que cada host puede tener múltiples "hijos" (entradas).
 */
struct Entry {
    string date;    // Mes + día (ej. "Apr 29")
    string time;    // Hora (ej. "10:09:17")
    string port;    // Puerto en texto (ej. "3946")
    string message; // Mensaje de error completo
};

/*
 * struct Host
 * Representa un nodo host (IP completa sin puerto) en el grafo.
 *
 * Campos:
 *  - ip: cadena con la IP completa (ej. "119.232.101.246")
 *  - entries: arreglo dinámico de entradas (hijos en el nivel más bajo)
 *  - entryCount: cuántas entradas tiene este host
 *  - entryCap: capacidad actual del arreglo dinámico entries
 *  - used: indica si esta posición de la tabla hash está ocupada
 *
 * El "grado de salida" efectivo de este host en el contexto de la actividad
 * se interpreta como entryCount (número de veces que aparece en la bitácora).
 */
struct Host {
    string ip;
    Entry* entries;
    int entryCount;
    int entryCap;
    bool used;
};

/*
 * struct Network
 * Representa un nodo red (primeros dos octetos de la IP) en el grafo.
 *
 * Campos:
 *  - prefix: cadena que identifica la red (ej. "119.232")
 *  - uniqueHostCount: número de hosts únicos que pertenecen a esta red
 *  - used: indica si esta posición de la tabla hash está ocupada
 *
 * El "grado de salida" de este nodo red es uniqueHostCount.
 */
struct Network {
    string prefix;
    int uniqueHostCount;
    bool used;
};

// -----------------------------------------------------------------------------
// 2. Tablas hash globales
// -----------------------------------------------------------------------------

/*
 * Tablas hash para almacenar hosts y redes.
 * Se implementan como arreglos de tamaño fijo + direccionamiento abierto
 * (linear probing).
 *
 * Clave lógica:
 *  - hostTable: clave = IP completa (string), valor = struct Host.
 *  - networkTable: clave = prefijo de red (string), valor = struct Network.
 */
Host hostTable[TABLE_SIZE];
Network networkTable[TABLE_SIZE];

// -----------------------------------------------------------------------------
// 3. Funciones auxiliares
// -----------------------------------------------------------------------------

/*
 * 3.1 hashString
 * Función de hash sencilla para strings basada en multiplicador 131.
 *
 * Recorre el string carácter por carácter y acumula en un entero sin signo.
 *
 * Parámetros:
 *  - s: referencia a la cadena a hashear.
 *
 * Regresa:
 *  - un valor entero sin signo que será reducido módulo TABLE_SIZE.
 *
 * Complejidad:
 *  - O(L), donde L es la longitud de la cadena s (pequeña en este contexto).
 */
unsigned int hashString(const string& s) {
    unsigned int h = 0;
    for (int i = 0; i < (int)s.size(); i++) {
        h = h * 131u + (unsigned char)s[i];
    }
    return h;
}

/*
 * 3.2 prefixFromIP
 * Obtiene el prefijo de red a partir de una IP completa.
 *
 * En este caso, se define la "red" como los primeros dos octetos de la IP:
 *   ejemplo: "119.232.101.246" -> "119.232"
 *
 * Implementación:
 *  - Busca el primer '.' y el segundo '.' en la cadena.
 *  - Devuelve la subcadena desde el inicio hasta antes del segundo '.'.
 *
 * Parámetros:
 *  - ip: cadena con la dirección IP completa.
 *
 * Regresa:
 *  - string con el prefijo de red (dos octetos) o la IP original si no se
 *    encuentran suficientes puntos.
 *
 * Complejidad:
 *  - O(L), donde L es la longitud de la IP (pequeña, L < 20 típicamente).
 */
string prefixFromIP(const string& ip) {
    size_t p1 = ip.find('.');
    if (p1 == string::npos) return ip;
    size_t p2 = ip.find('.', p1 + 1);
    if (p2 == string::npos) return ip;
    return ip.substr(0, p2);
}

/*
 * 3.3 getNetworkIndex
 * Inserta o recupera el índice de una red dentro de networkTable.
 *
 * Lógica:
 *  - Calcula el hash del prefijo.
 *  - Aplica módulo TABLE_SIZE para obtener índice inicial.
 *  - Usa direccionamiento abierto (linear probing):
 *      - Si la casilla está libre (used == false), crea un nuevo Network.
 *      - Si la casilla tiene la misma prefix, regresa ese índice.
 *      - Si la casilla está ocupada por otro prefix, avanza al siguiente índice.
 *  - Se limita a TABLE_SIZE pasos para evitar bucles infinitos.
 *
 * Parámetros:
 *  - prefix: identificador de la red (ej. "119.232").
 *
 * Regresa:
 *  - índice entero en networkTable donde se encuentra (o se creó) la red.
 *
 * Complejidad:
 *  - Promedio: O(1) por operación (búsqueda/inserción en hash).
 *  - Peor caso: O(TABLE_SIZE) si hay muchas colisiones.
 */
int getNetworkIndex(const string& prefix) {
    unsigned int h = hashString(prefix) % TABLE_SIZE;
    int pasos = 0;

    while (pasos < TABLE_SIZE) {
        if (!networkTable[h].used) {
            // Casilla libre: creamos nueva red
            networkTable[h].used = true;
            networkTable[h].prefix = prefix;
            networkTable[h].uniqueHostCount = 0;
            return h;
        }
        if (networkTable[h].prefix == prefix) {
            // Red ya existente, regresamos índice
            return h;
        }
        // Colisión: avanzamos (linear probing)
        h++;
        if (h == (unsigned int)TABLE_SIZE) h = 0;
        pasos++;
    }

    // Si se recorre toda la tabla, significa que está llena (caso extremo)
    cerr << "Error: tabla de redes llena, aumenta TABLE_SIZE\n";
    exit(1);
}

/*
 * 3.4 getHostIndex
 * Inserta o recupera el índice de un host dentro de hostTable.
 *
 * Lógica:
 *  - Calcula el hash de la IP (string).
 *  - Aplica módulo TABLE_SIZE para obtener índice inicial.
 *  - Usa linear probing para resolver colisiones:
 *      - Si la casilla está libre: se inicializa el Host con esa IP.
 *      - Si la casilla tiene la misma IP: se regresa ese índice.
 *      - Si la casilla tiene otra IP: se avanza al siguiente índice.
 *  - Se limita a TABLE_SIZE pasos para evitar bucles infinitos.
 *
 * Parámetros:
 *  - ip: cadena con la IP completa (sin puerto).
 *  - isNew: referencia a bool, se coloca en true si el host se acaba de crear,
 *           false si ya existía.
 *
 * Regresa:
 *  - índice entero en hostTable donde se almacena el Host asociado a esa IP.
 *
 * Complejidad:
 *  - Promedio: O(1) por operación.
 *  - Peor caso: O(TABLE_SIZE).
 */
int getHostIndex(const string& ip, bool& isNew) {
    unsigned int h = hashString(ip) % TABLE_SIZE;
    int pasos = 0;

    while (pasos < TABLE_SIZE) {
        if (!hostTable[h].used) {
            // Casilla libre: inicializamos nuevo host
            hostTable[h].used = true;
            hostTable[h].ip = ip;
            hostTable[h].entryCap = 10;          // capacidad inicial del arreglo de entradas
            hostTable[h].entryCount = 0;
            hostTable[h].entries = new Entry[hostTable[h].entryCap];
            isNew = true;
            return h;
        }
        if (hostTable[h].ip == ip) {
            // Host ya existente
            isNew = false;
            return h;
        }
        // Colisión: linear probing
        h++;
        if (h == (unsigned int)TABLE_SIZE) h = 0;
        pasos++;
    }

    cerr << "Error: tabla de hosts llena, aumenta TABLE_SIZE\n";
    exit(1);
}

// -----------------------------------------------------------------------------
// 4. Función principal (main)
// -----------------------------------------------------------------------------

int main() {
    // 4.1 Inicialización de tablas hash
    /*
     * Se marcan todas las posiciones como "no usadas" y se inicializan
     * contadores y punteros.
     *
     * Complejidad:
     *  - O(TABLE_SIZE), se recorre una vez cada tabla.
     */
    for (int i = 0; i < TABLE_SIZE; i++) {
        hostTable[i].used = false;
        hostTable[i].entries = NULL;
        hostTable[i].entryCap = 0;
        hostTable[i].entryCount = 0;

        networkTable[i].used = false;
        networkTable[i].uniqueHostCount = 0;
        networkTable[i].prefix = "";
    }

    // 4.2 Apertura del archivo de bitácora
    /*
     * Se abre el archivo "bitacora.txt" en modo lectura.
     * El nombre está fijo, como lo indican las instrucciones de la actividad.
     */
    ifstream file("bitacora.txt");
    if (!file.is_open()) {
        cerr << "No se pudo abrir bitacora.txt\n";
        return 1;
    }

    // 4.3 Lectura línea por línea y construcción del grafo lógico
    /*
     * Por cada línea:
     *  - Se extraen: mes, día, hora, "IP:PORT".
     *  - Se separa la IP del puerto.
     *  - Se obtiene el prefijo de red (primeros dos octetos).
     *  - Se asegura la existencia del host (getHostIndex).
     *      - Si es nuevo host: se incrementa el contador de hosts únicos
     *        de su red correspondiente.
     *  - Se agrega la entrada (Entry) al arreglo de ese host.
     *
     * Complejidad:
     *  - Sea N el número de líneas del archivo.
     *  - Cada línea se procesa en tiempo O(L) (L = longitud de la línea).
     *  - Cada getHostIndex / getNetworkIndex es O(1) amortizado.
     *  - Complejidad total del bucle: O(N * L) ~ O(N).
     */
    string line;
    while (getline(file, line)) {
        if (line.empty()) {
            continue; // línea vacía, se omite
        }

        istringstream iss(line);
        string month, day, timeStr, ipPort;

        // Intentamos leer: mes, día, hora, y "IP:PORT"
        if (!(iss >> month >> day >> timeStr >> ipPort)) {
            // Línea mal formada, no se puede procesar correctamente
            continue;
        }

        // El resto de la línea es el mensaje (puede tener espacios)
        string message;
        getline(iss, message);
        if (!message.empty() && message[0] == ' ') {
            // Eliminamos espacio inicial sobrante
            message.erase(0, 1);
        }

        // 4.3.1 Separar IP y puerto a partir de "IP:PORT"
        /*
         * Buscamos ':' en la cadena ipPort:
         *  - Si existe: lo que está antes es la IP, lo que está después es el puerto.
         *  - Si no existe: tomamos todo como IP y puerto vacío.
         */
        string ip;
        string port;
        size_t colonPos = ipPort.find(':');
        if (colonPos != string::npos) {
            ip = ipPort.substr(0, colonPos);
            port = ipPort.substr(colonPos + 1);
        } else {
            ip = ipPort;
            port = "";
        }

        // 4.3.2 Obtener prefijo de red (dos primeros octetos)
        string prefix = prefixFromIP(ip);

        // 4.3.3 Insertar / obtener host en tabla hash
        bool isNewHost;
        int hostIndex = getHostIndex(ip, isNewHost);

        // 4.3.4 Si el host es nuevo, asociarlo a su red y aumentar contador
        /*
         * Para reflejar el grafo:
         *   Red (prefix) -> Host (ip)
         * Solo se incrementa uniqueHostCount la primera vez que vemos este host.
         */
        if (isNewHost) {
            int netIndex = getNetworkIndex(prefix);
            networkTable[netIndex].uniqueHostCount++;
        }

        // 4.3.5 Agregar entrada (Entry) al host correspondiente
        /*
         * Se utiliza un arreglo dinámico "entries" que se redimensiona
         * cuando se llena (doblando su capacidad).
         *
         * Complejidad de cada inserción:
         *  - Amortizada O(1), ya que el redimensionamiento es poco frecuente.
         */
        Host& h = hostTable[hostIndex];
        if (h.entryCount == h.entryCap) {
            int newCap = (h.entryCap == 0) ? 10 : h.entryCap * 2;
            Entry* newArr = new Entry[newCap];
            for (int i = 0; i < h.entryCount; i++) {
                newArr[i] = h.entries[i];
            }
            delete[] h.entries;
            h.entries = newArr;
            h.entryCap = newCap;
        }

        Entry& e = h.entries[h.entryCount];
        e.date = month + " " + day;
        e.time = timeStr;
        e.port = port;
        e.message = message;
        h.entryCount++;
    }

    file.close();

    // -------------------------------------------------------------------------
    // 4.4 Cálculo de redes con mayor número de hosts únicos
    // -------------------------------------------------------------------------

    /*
     * Primero encontramos el máximo valor de uniqueHostCount en networkTable.
     *
     * Complejidad:
     *  - O(TABLE_SIZE), se recorre la tabla una vez.
     */
    int maxHosts = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (networkTable[i].used && networkTable[i].uniqueHostCount > maxHosts) {
            maxHosts = networkTable[i].uniqueHostCount;
        }
    }

    /*
     * Luego imprimimos todas las redes cuyo uniqueHostCount == maxHosts.
     * Esto cumple con la especificación de imprimir "la red o redes con
     * mayor número de hosts registrados".
     *
     * Complejidad:
     *  - O(TABLE_SIZE), se recorre de nuevo la tabla.
     */
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (networkTable[i].used && networkTable[i].uniqueHostCount == maxHosts) {
            cout << networkTable[i].prefix << "\n";
        }
    }
    cout << "\n"; // Línea en blanco entre redes y hosts, tal como pide el ejemplo

    // -------------------------------------------------------------------------
    // 4.5 Cálculo de hosts (IP completa) con mayor número de entradas
    // -------------------------------------------------------------------------

    /*
     * Primero encontramos el máximo número de entradas (entryCount) en hostTable.
     *
     * Complejidad:
     *  - O(TABLE_SIZE), se recorre una vez la tabla de hosts.
     */
    int maxEntries = 0;
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (hostTable[i].used && hostTable[i].entryCount > maxEntries) {
            maxEntries = hostTable[i].entryCount;
        }
    }

    /*
     * Después, imprimimos todas las IPs cuyo entryCount == maxEntries.
     * Estas corresponden a los hosts con mayor incidencia (más entradas
     * en el archivo de bitácora), como lo pide la actividad.
     *
     * Complejidad:
     *  - O(TABLE_SIZE), otro recorrido sobre hostTable.
     */
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (hostTable[i].used && hostTable[i].entryCount == maxEntries) {
            cout << hostTable[i].ip << "\n";
        }
    }

    // -------------------------------------------------------------------------
    // 4.6 Liberación de memoria dinámica
    // -------------------------------------------------------------------------

    /*
     * Se libera la memoria del arreglo dinámico entries de cada host usado.
     * Esto es buena práctica para evitar fugas de memoria.
     *
     * Complejidad:
     *  - O(TABLE_SIZE), en el peor caso, si muchos hosts están usados.
     */
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (hostTable[i].used && hostTable[i].entries != NULL) {
            delete[] hostTable[i].entries;
            hostTable[i].entries = NULL;
        }
    }

    return 0;
}