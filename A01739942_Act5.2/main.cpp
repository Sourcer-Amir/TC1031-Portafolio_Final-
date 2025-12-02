/*
    Descripción general:
    Programa que lee un archivo de bitácora (bitacora.txt) y construye una
    tabla hash donde:
    - La llave es la red (primeros dos octetos de la dirección IP, ej. "145.25")
    - El valor es un resumen que incluye:
        * Número total de accesos desde esa red
        * Número de IPs únicas (conexiones) en esa red
        * Lista de IPs únicas ordenadas de manera ascendente (natural, no ajustada)

    El tamaño de la tabla hash es 65521 (número primo más grande menor a 65536).
    
    Se utiliza Linear Probing para resolver colisiones.
    
    El programa recibe N consultas de redes y despliega el resumen de cada una.

    Restricciones:
    - No se usan vector, algorithm, unordered_map, etc.
    - Solo se utilizan: <iostream>, <fstream>, <sstream>, <string>.
    - El archivo se llama exactamente "bitacora.txt" y no se pide al usuario.

    Complejidad general aproximada:
    - Lectura y procesamiento de N líneas: O(N)
    - Inserción/búsqueda en tabla hash: O(1) promedio
    - Ordenamiento de IPs por red: O(M log M) donde M es el número de IPs en esa red
    - Complejidad total: O(N + M log M) en tiempo y O(TABLE_SIZE) en espacio
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
 * Tamaño de la tabla hash.
 * Debe ser el número primo más grande menor a 65536, que es 65521.
 * Este tamaño minimiza colisiones y distribuye uniformemente las redes.
 *
 * Espacio: O(TABLE_SIZE)
 */
const int TABLE_SIZE = 65521;

/*
 * struct IPNode
 * Representa un nodo en una lista enlazada de direcciones IP únicas.
 * Se utiliza para almacenar las IPs que pertenecen a una misma red.
 *
 * Campos:
 *  - ip: cadena con la dirección IP completa (ej. "145.25.32.15")
 *  - next: puntero al siguiente nodo en la lista
 */
struct IPNode {
    string ip;
    IPNode* next;
};

/*
 * struct NetworkInfo
 * Representa la información de una red en la tabla hash.
 *
 * Campos:
 *  - network: identificador de la red (primeros dos octetos, ej. "145.25")
 *  - accessCount: número total de accesos desde esta red
 *  - uniqueIPs: puntero a lista enlazada de IPs únicas en esta red
 *  - connectionCount: número de IPs únicas (conexiones distintas)
 *  - occupied: indica si esta posición de la tabla hash está ocupada
 */
struct NetworkInfo {
    string network;
    int accessCount;
    IPNode* uniqueIPs;
    int connectionCount;
    bool occupied;
};

// -----------------------------------------------------------------------------
// 2. Tabla hash global
// -----------------------------------------------------------------------------

/*
 * Tabla hash para almacenar información de redes.
 * Se implementa como arreglo de tamaño fijo con direccionamiento abierto
 * (Linear Probing) para resolver colisiones.
 *
 * Clave lógica: identificador de red (string con dos octetos)
 * Valor: struct NetworkInfo con toda la información de esa red
 */
NetworkInfo hashTable[TABLE_SIZE];

// Contador de elementos en la tabla
int itemCount = 0;

// -----------------------------------------------------------------------------
// 3. Funciones auxiliares
// -----------------------------------------------------------------------------

/*
 * 3.1 hashFunction
 * Función hash que convierte una cadena (identificador de red) en un índice
 * de la tabla hash.
 *
 * JUSTIFICACIÓN DE LA FUNCIÓN HASH:
 * Se utiliza el método de multiplicación con dos constantes primas (31 y 37):
 * 1. Se itera sobre cada carácter de la cadena
 * 2. Se multiplica el hash acumulado por la primera prima (31)
 * 3. Se suma el valor ASCII del carácter actual
 * 4. Se aplica módulo TABLE_SIZE para mantener el resultado en rango
 * 5. Se suma la segunda prima (37) para mejorar distribución
 * 6. Se vuelve a aplicar módulo
 *
 * Esta técnica proporciona:
 * - Buena distribución uniforme de valores hash
 * - Reducción significativa de colisiones
 * - Aprovechamiento eficiente del espacio de la tabla
 * - Sensibilidad a todos los caracteres de la clave
 *
 * Las constantes primas ayudan a evitar patrones que generarían colisiones
 * sistemáticas. El uso de dos primas diferentes en dos pasos distintos
 * mejora aún más la distribución.
 *
 * Parámetros:
 *  - key: referencia a la cadena que representa la red
 *
 * Regresa:
 *  - índice entero en el rango [0, TABLE_SIZE-1]
 *
 * Complejidad:
 *  - O(L), donde L es la longitud de la cadena (pequeña, típicamente < 10)
 */
int hashFunction(const string& key) {
    unsigned long hash = 0;
    const int PRIME1 = 31;
    const int PRIME2 = 37;
    
    for (int i = 0; i < (int)key.length(); i++) {
        hash = (hash * PRIME1 + (unsigned char)key[i]) % TABLE_SIZE;
        hash = (hash + PRIME2) % TABLE_SIZE;
    }
    
    return hash % TABLE_SIZE;
}

/*
 * 3.2 extractNetwork
 * Extrae el identificador de red a partir de una dirección IP completa.
 * La red se define como los primeros dos octetos de la IP.
 *
 * Implementación:
 *  - Busca el primer punto en la cadena
 *  - Busca el segundo punto después del primero
 *  - Devuelve la subcadena desde el inicio hasta antes del segundo punto
 *
 * Ejemplo: "145.25.32.15" -> "145.25"
 *
 * Parámetros:
 *  - ip: cadena con la dirección IP completa
 *
 * Regresa:
 *  - string con el identificador de red (dos octetos)
 *  - Si no se encuentran suficientes puntos, devuelve la IP original
 *
 * Complejidad:
 *  - O(L), donde L es la longitud de la IP (típicamente L < 20)
 */
string extractNetwork(const string& ip) {
    size_t firstDot = ip.find('.');
    if (firstDot == string::npos) return "";
    
    size_t secondDot = ip.find('.', firstDot + 1);
    if (secondDot == string::npos) return "";
    
    return ip.substr(0, secondDot);
}

/*
 * 3.3 ipExists
 * Verifica si una dirección IP ya existe en la lista enlazada de IPs únicas.
 *
 * Implementación:
 *  - Recorre la lista enlazada desde el inicio
 *  - Compara cada IP con la buscada
 *  - Retorna true si encuentra coincidencia, false si llega al final
 *
 * Parámetros:
 *  - head: puntero al primer nodo de la lista enlazada
 *  - ip: cadena con la IP a buscar
 *
 * Regresa:
 *  - true si la IP existe en la lista
 *  - false si no existe
 *
 * Complejidad:
 *  - O(M), donde M es el número de IPs en la lista (típicamente pequeño)
 */
bool ipExists(IPNode* head, const string& ip) {
    IPNode* current = head;
    while (current != NULL) {
        if (current->ip == ip) {
            return true;
        }
        current = current->next;
    }
    return false;
}

/*
 * 3.4 addIP
 * Agrega una dirección IP al inicio de la lista enlazada de IPs únicas.
 *
 * Implementación:
 *  - Crea un nuevo nodo con la IP
 *  - Lo conecta al inicio de la lista existente
 *  - Actualiza el puntero head
 *
 * Parámetros:
 *  - head: referencia al puntero del primer nodo (se modifica)
 *  - ip: cadena con la IP a agregar
 *
 * Complejidad:
 *  - O(1), inserción al inicio de lista enlazada
 */
void addIP(IPNode*& head, const string& ip) {
    IPNode* newNode = new IPNode;
    newNode->ip = ip;
    newNode->next = head;
    head = newNode;
}

/*
 * 3.5 parseIPOctets
 * Convierte una dirección IP (string) en un arreglo de 4 enteros.
 * Cada entero representa un octeto de la IP.
 *
 * Implementación:
 *  - Usa stringstream para separar por puntos
 *  - Convierte cada parte a entero
 *
 * Ejemplo: "145.25.32.15" -> [145, 25, 32, 15]
 *
 * Parámetros:
 *  - ip: cadena con la dirección IP
 *  - octets: arreglo de salida donde se almacenan los 4 octetos
 *  - count: referencia donde se guarda el número de octetos procesados
 *
 * Complejidad:
 *  - O(L), donde L es la longitud de la IP (pequeña)
 */
void parseIPOctets(const string& ip, int octets[], int& count) {
    count = 0;
    stringstream ss(ip);
    string part;
    
    while (getline(ss, part, '.') && count < 4) {
        octets[count++] = 0;
        for (int i = 0; i < (int)part.length(); i++) {
            octets[count - 1] = octets[count - 1] * 10 + (part[i] - '0');
        }
    }
}

/*
 * 3.6 compareIPs
 * Compara dos direcciones IP de manera numérica (no lexicográfica).
 *
 * Implementación:
 *  - Convierte ambas IPs a arreglos de octetos
 *  - Compara octeto por octeto de izquierda a derecha
 *  - Retorna true si ip1 < ip2 numéricamente
 *
 * Esto asegura ordenamiento natural: 145.25.32.15 < 145.25.178.65
 * (no ordenamiento alfabético donde "178" < "32")
 *
 * Parámetros:
 *  - ip1: primera dirección IP
 *  - ip2: segunda dirección IP
 *
 * Regresa:
 *  - true si ip1 es menor que ip2 numéricamente
 *  - false en caso contrario
 *
 * Complejidad:
 *  - O(1), ya que siempre se comparan exactamente 4 octetos
 */
bool compareIPs(const string& ip1, const string& ip2) {
    int octets1[4], octets2[4];
    int count1, count2;
    
    parseIPOctets(ip1, octets1, count1);
    parseIPOctets(ip2, octets2, count2);
    
    for (int i = 0; i < 4 && i < count1 && i < count2; i++) {
        if (octets1[i] != octets2[i]) {
            return octets1[i] < octets2[i];
        }
    }
    
    return count1 < count2;
}

/*
 * 3.7 sortIPList
 * Ordena una lista enlazada de direcciones IP usando el algoritmo de
 * ordenamiento por inserción (Insertion Sort).
 *
 * Implementación:
 *  - Construye una nueva lista ordenada nodo por nodo
 *  - Para cada nodo de la lista original:
 *      * Lo inserta en la posición correcta de la lista ordenada
 *      * Mantiene el orden ascendente usando compareIPs
 *
 * Parámetros:
 *  - head: referencia al puntero del primer nodo (se modifica)
 *
 * Complejidad:
 *  - O(M²) en peor caso, donde M es el número de IPs en la lista
 *  - Para listas pequeñas (típico en este problema), es eficiente
 */
void sortIPList(IPNode*& head) {
    if (head == NULL || head->next == NULL) {
        return; // Lista vacía o con un solo elemento
    }
    
    IPNode* sorted = NULL;
    IPNode* current = head;
    
    while (current != NULL) {
        IPNode* next = current->next;
        
        // Insertar current en la lista ordenada
        if (sorted == NULL || compareIPs(current->ip, sorted->ip)) {
            current->next = sorted;
            sorted = current;
        } else {
            IPNode* search = sorted;
            while (search->next != NULL && compareIPs(search->next->ip, current->ip)) {
                search = search->next;
            }
            current->next = search->next;
            search->next = current;
        }
        
        current = next;
    }
    
    head = sorted;
}

/*
 * 3.8 insertOrUpdate
 * Inserta una nueva red en la tabla hash o actualiza una existente.
 * Utiliza Linear Probing para resolver colisiones.
 *
 * Lógica:
 *  - Calcula el índice hash para la red
 *  - Si la celda está vacía: crea nueva NetworkInfo
 *  - Si la celda tiene la misma red: actualiza accessCount y agrega IP si es única
 *  - Si la celda está ocupada por otra red: avanza a la siguiente (Linear Probing)
 *  - Si la tabla está llena: retorna false
 *
 * Parámetros:
 *  - network: identificador de la red
 *  - ip: dirección IP completa que accedió
 *
 * Regresa:
 *  - true si se insertó/actualizó correctamente
 *  - false si la tabla está llena
 *
 * Complejidad:
 *  - Promedio: O(1) por operación (con buen factor de carga)
 *  - Peor caso: O(TABLE_SIZE) si hay muchas colisiones
 */
bool insertOrUpdate(const string& network, const string& ip) {
    if (itemCount >= TABLE_SIZE) {
        return false; // Tabla llena
    }
    
    int index = hashFunction(network);
    int originalIndex = index;
    int probeCount = 0;
    
    // Linear Probing para manejar colisiones
    while (probeCount < TABLE_SIZE) {
        if (!hashTable[index].occupied) {
            // Celda vacía: crear nueva red
            hashTable[index].occupied = true;
            hashTable[index].network = network;
            hashTable[index].accessCount = 1;
            hashTable[index].uniqueIPs = NULL;
            hashTable[index].connectionCount = 0;
            
            addIP(hashTable[index].uniqueIPs, ip);
            hashTable[index].connectionCount = 1;
            
            itemCount++;
            return true;
        } else if (hashTable[index].network == network) {
            // Red ya existe: actualizar
            hashTable[index].accessCount++;
            
            // Agregar IP solo si no existe
            if (!ipExists(hashTable[index].uniqueIPs, ip)) {
                addIP(hashTable[index].uniqueIPs, ip);
                hashTable[index].connectionCount++;
            }
            return true;
        }
        
        // Colisión: probar siguiente posición
        index = (index + 1) % TABLE_SIZE;
        probeCount++;
        
        if (index == originalIndex) {
            break; // Dimos la vuelta completa
        }
    }
    
    return false; // Tabla llena
}

/*
 * 3.9 searchNetwork
 * Busca una red en la tabla hash usando Linear Probing.
 *
 * Lógica:
 *  - Calcula el índice hash de la red
 *  - Si encuentra una celda vacía: la red no existe
 *  - Si encuentra la red buscada: retorna su índice
 *  - Si encuentra otra red: continúa buscando (Linear Probing)
 *
 * Parámetros:
 *  - network: identificador de la red a buscar
 *
 * Regresa:
 *  - índice de la red en la tabla si existe
 *  - -1 si no se encuentra
 *
 * Complejidad:
 *  - Promedio: O(1)
 *  - Peor caso: O(TABLE_SIZE)
 */
int searchNetwork(const string& network) {
    int index = hashFunction(network);
    int originalIndex = index;
    int probeCount = 0;
    
    while (probeCount < TABLE_SIZE) {
        if (!hashTable[index].occupied) {
            return -1; // No encontrada
        }
        
        if (hashTable[index].network == network) {
            return index; // Encontrada
        }
        
        index = (index + 1) % TABLE_SIZE;
        probeCount++;
        
        if (index == originalIndex) {
            break;
        }
    }
    
    return -1; // No encontrada
}

/*
 * 3.10 freeIPList
 * Libera la memoria de una lista enlazada de direcciones IP.
 *
 * Implementación:
 *  - Recorre la lista nodo por nodo
 *  - Libera cada nodo con delete
 *
 * Parámetros:
 *  - head: puntero al primer nodo de la lista
 *
 * Complejidad:
 *  - O(M), donde M es el número de nodos en la lista
 */
void freeIPList(IPNode* head) {
    while (head != NULL) {
        IPNode* temp = head;
        head = head->next;
        delete temp;
    }
}

// -----------------------------------------------------------------------------
// 4. Función principal (main)
// -----------------------------------------------------------------------------

int main() {
    // 4.1 Inicialización de la tabla hash
    /*
     * Se marcan todas las posiciones como no ocupadas.
     *
     * Complejidad:
     *  - O(TABLE_SIZE)
     */
    for (int i = 0; i < TABLE_SIZE; i++) {
        hashTable[i].occupied = false;
        hashTable[i].network = "";
        hashTable[i].accessCount = 0;
        hashTable[i].uniqueIPs = NULL;
        hashTable[i].connectionCount = 0;
    }
    
    // 4.2 Apertura del archivo de bitácora
    /*
     * Se abre el archivo "bitacora.txt" en modo lectura.
     * El nombre está fijo según las instrucciones de la actividad.
     */
    ifstream file("bitacora.txt");
    
    if (!file.is_open()) {
        cerr << "Error: No se pudo abrir el archivo bitacora.txt" << endl;
        return 1;
    }
    
    // 4.3 Lectura línea por línea y construcción de la tabla hash
    /*
     * Por cada línea:
     *  - Se extraen: mes, día, hora, "IP:PUERTO"
     *  - Se separa la IP del puerto
     *  - Se obtiene el identificador de red (primeros dos octetos)
     *  - Se inserta o actualiza en la tabla hash
     *
     * Complejidad:
     *  - Sea N el número de líneas del archivo
     *  - Cada línea se procesa en tiempo O(L) donde L es su longitud
     *  - Cada insertOrUpdate es O(1) amortizado
     *  - Complejidad total: O(N)
     */
    string line;
    while (getline(file, line)) {
        if (line.empty()) {
            continue; // Línea vacía, se omite
        }
        
        stringstream ss(line);
        string month, day, time, ipPort;
        
        // Leer: mes, día, hora, "IP:PUERTO"
        if (!(ss >> month >> day >> time >> ipPort)) {
            continue; // Línea mal formada
        }
        
        // Separar IP y puerto a partir de "IP:PUERTO"
        size_t colonPos = ipPort.find(':');
        string ip = (colonPos != string::npos) ? ipPort.substr(0, colonPos) : ipPort;
        
        // Extraer identificador de red
        string network = extractNetwork(ip);
        
        if (!network.empty()) {
            if (!insertOrUpdate(network, ip)) {
                cerr << "Error: Tabla llena, imposible meter más datos" << endl;
                file.close();
                
                // Liberar memoria antes de salir
                for (int i = 0; i < TABLE_SIZE; i++) {
                    if (hashTable[i].occupied) {
                        freeIPList(hashTable[i].uniqueIPs);
                    }
                }
                
                return 1;
            }
        }
    }
    
    file.close();
    
    // 4.4 Procesamiento de consultas
    /*
     * Lee N consultas de redes y despliega el resumen de cada una.
     *
     * Para cada consulta:
     *  - Busca la red en la tabla hash
     *  - Si existe: ordena sus IPs y despliega el resumen
     *  - Si no existe: indica que no se encontró
     *
     * Complejidad por consulta:
     *  - Búsqueda: O(1) promedio
     *  - Ordenamiento: O(M² ) donde M es el número de IPs en esa red
     *  - Impresión: O(M)
     *  - Total por consulta: O(M²)
     */
    int n;
    cin >> n;
    
    for (int i = 0; i < n; i++) {
        string queryNetwork;
        cin >> queryNetwork;
        
        int index = searchNetwork(queryNetwork);
        
        if (index == -1) {
            cout << queryNetwork << endl;
            cout << "Red no encontrada" << endl;
        } else {
            // Ordenar las IPs de manera ascendente
            sortIPList(hashTable[index].uniqueIPs);
            
            // Imprimir resultados
            cout << hashTable[index].network << endl;
            cout << hashTable[index].accessCount << endl;
            cout << hashTable[index].connectionCount << endl;
            
            IPNode* current = hashTable[index].uniqueIPs;
            while (current != NULL) {
                cout << current->ip << endl;
                current = current->next;
            }
        }
        
        // Línea en blanco entre consultas (excepto la última)
        if (i < n - 1) {
            cout << endl;
        }
    }
    
    // 4.5 Liberación de memoria dinámica
    /*
     * Se libera la memoria de todas las listas enlazadas de IPs.
     *
     * Complejidad:
     *  - O(TABLE_SIZE + total de IPs almacenadas)
     */
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (hashTable[i].occupied) {
            freeIPList(hashTable[i].uniqueIPs);
        }
    }
    
    return 0;
}