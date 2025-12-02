/*
    Descripción: Programa que lee un archivo de bitácora, almacena los registros en una lista doblemente ligada,
    los ordena por dirección IP (numéricamente) y permite buscar registros en un rango de IPs, 
    desplegando los resultados en orden descendente. También guarda la lista ordenada completa en "SortedData.txt".

    [Ayleen Osnaya Ortega] - [A01426008]
    [José Luis Gutiérrez Quintero] - [A01739337]
    [Santiago Amir Rodríguez González] - [A01739942]
    Fecha: 15/10/2025
*/

#include <iostream>
#include <fstream>
#include <string>
using namespace std;

/* ---------------- 1. ESTRUCTURA PRINCIPAL ----------------
 * Representa un registro de bitácora y nodo de la lista doblemente ligada.
 * entry: campos parseados de una línea (fecha, hora, IP, puerto, motivo).
 * Node: nodo de la lista (con punteros prev y next para lista doblemente ligada).
 * Se almacena también la línea original completa para impresión.
 */
struct entry {
    int month, day, hour, min, sec;    // Fecha y hora desglosada
    long long totalTime;              // Clave numérica de fecha/hora para comparar fácilmente
    int ip1, ip2, ip3, ip4;           // Octetos de la IP
    int port;                        // Puerto de la conexión
    string reason;                   // Mensaje de error o descripción
    string originLine;               // Línea original completa (útil para imprimir exactamente igual)
};

struct Node {
    entry data;
    Node* prev;
    Node* next;
    Node(const entry &e) : data(e), prev(nullptr), next(nullptr) {}
};


/* ---------------- 2. FUNCIONES AUXILIARES ---------------- */

/*
 * 2.1 months_int
 * Convierte las abreviaturas de mes en número (Jan=1, Feb=2, ..., Dec=12).
 * Devuelve -1 si el mes no es válido (no debería ocurrir en datos válidos).
 * Complejidad: O(1) por comparar hasta 12 elementos.
 */
int months_int(const string &month) {
    string months[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    for(int i = 0; i < 12; i++) {
        if(months[i] == month)
            return i + 1;
    }
    return -1;
}

/*
 * 2.2 tokenizer
 * Extrae el siguiente token (secuencia de caracteres hasta el siguiente espacio) de la línea dada,
 * a partir de la posición pos. Actualiza pos a la posición después del token extraído.
 * Si no se encuentra más espacios, devuelve el resto de la línea.
 * Complejidad: O(n) en el peor caso (n = longitud restante de la línea).
 */
string tokenizer(const string &s, size_t &pos) {
    if(pos >= s.size()) return "";               // no more tokens
    size_t start = pos;
    size_t found = s.find(' ', pos);
    if(found == string::npos) {
        // No se encontraron más espacios, devolver el resto de la línea
        pos = s.size();
        return s.substr(start);
    } else {
        // Encontró un espacio - extraer token hasta ese espacio
        pos = found + 1;
        return s.substr(start, found - start);
    }
}

/*
 * 2.3 splitIp
 * Divide una cadena "IP:PORT" en sus componentes numéricos.
 * Parámetros de salida: a,b,c,d corresponden a los 4 octetos de la IP, y p al puerto.
 * Complejidad: O(k), donde k es la longitud de la cadena IP:PORT (muy pequeña, k <  veinte caracteres típicamente).
 */
void splitIp(const string &ipPort, int &a, int &b, int &c, int &d, int &p) {
    // Separa IP y puerto si existe ':'
    string ipStr;
    size_t colon = ipPort.find(':');
    if (colon == string::npos) {
        ipStr = ipPort;
        p = 0;  // sin puerto
    } else {
        ipStr = ipPort.substr(0, colon);
        // manejar puerto solo si hay algo tras ':'
        string portStr = ipPort.substr(colon + 1);
        p = portStr.empty() ? 0 : stoi(portStr);
    }

    // Parsear los 4 octetos de la IP
    size_t pos = 0, next;
    // octeto 1
    next = ipStr.find('.', pos);
    a = stoi(ipStr.substr(pos, next - pos));
    pos = (next == string::npos) ? ipStr.size() : next + 1;
    // octeto 2
    next = ipStr.find('.', pos);
    b = stoi(ipStr.substr(pos, next - pos));
    pos = (next == string::npos) ? ipStr.size() : next + 1;
    // octeto 3
    next = ipStr.find('.', pos);
    c = stoi(ipStr.substr(pos, next - pos));
    pos = (next == string::npos) ? ipStr.size() : next + 1;
    // octeto 4 (resto)
    d = stoi(ipStr.substr(pos));
}


/*
 * 2.4 total_time
 * Calcula una clave numérica a partir de una fecha y hora desglosada.
 * Sirve para comparar rápidamente dos fechas/horas.
 * (Asume que las entradas están dentro del mismo año y rango razonable de meses).
 * Complejidad: O(1).
 */
long long total_time(int month, int day, int hour, int minute, int second) {
    return (((((long long)month * 31 + day) * 24 + hour) * 60 + minute) * 60 + second);
}

/*
 * 2.5 lessEntry
 * Comparador que define el orden requerido para dos registros.
 * Criterios de ordenamiento (de mayor prioridad a menor):
 * 1) Dirección IP (ip1, ip2, ip3, ip4) en orden numérico:contentReference[oaicite:13]{index=13}.
 * 2) Fecha y hora (totalTime) – más antiguos primero (ascendente).
 * 3) Mensaje de error (reason) – comparación lexicográfica como desempate final.
 * Devuelve true si 'a' debe ir antes que 'b' según este orden.
 * Complejidad: O(m) en el peor caso, donde m es la longitud de la cadena reason a comparar (comparación de strings).
 */
bool lessEntry(const entry &a, const entry &b) {
    if(a.ip1 != b.ip1) return a.ip1 < b.ip1;
    if(a.ip2 != b.ip2) return a.ip2 < b.ip2;
    if(a.ip3 != b.ip3) return a.ip3 < b.ip3;
    if(a.ip4 != b.ip4) return a.ip4 < b.ip4;
    // Si llega aquí, las IPs son iguales
    if(a.totalTime != b.totalTime) return a.totalTime < b.totalTime;
    // Si también la fecha/hora es igual, usar mensaje
    return a.reason < b.reason;
}

/*
 * 2.6 mergeSortedLists
 * Función auxiliar para mergeSort: fusiona dos sub-listas ordenadas (lista1 comenzando en 'first', lista2 en 'second')
 * en una sola lista ordenada según lessEntry. Devuelve el puntero al inicio de la lista resultante.
 * Complejidad: O(n) donde n es la suma de elementos en las dos sublistas.
 */
Node* mergeSortedLists(Node* first, Node* second) {
    if(!first) return second;
    if(!second) return first;
    // Determinar cabeza inicial de la lista fusionada
    Node* newHead = nullptr;
    Node* newTail = nullptr;
    // Mientas haya elementos en ambas listas, elegir el menor según lessEntry
    while(first && second) {
        if( lessEntry(first->data, second->data) ) {
            // 'first' va primero
            if(newHead == nullptr) {
                newHead = first;
            } else {
                newTail->next = first;
                first->prev = newTail;
            }
            newTail = first;
            first = first->next;
        } else {
            // 'second' va primero
            if(newHead == nullptr) {
                newHead = second;
            } else {
                newTail->next = second;
                second->prev = newTail;
            }
            newTail = second;
            second = second->next;
        }
    }
    // Adjuntar la lista restante (si quedó una sin terminar)
    if(first) {
        newTail->next = first;
        first->prev = newTail;
        // avanzar tail hasta el final de la lista 'first'
        while(first->next) first = first->next;
        newTail = first;
    }
    if(second) {
        newTail->next = second;
        second->prev = newTail;
        // avanzar tail hasta final de 'second'
        while(second->next) second = second->next;
        newTail = second;
    }
    // Asegurar que el último nodo apunte a nullptr y actualizar prev del head
    if(newHead) newHead->prev = nullptr;
    if(newTail) newTail->next = nullptr;
    return newHead;
}

/*
 * 2.7 mergeSortList
 * Implementa Merge Sort para la lista doblemente ligada.
 * Recibe el apuntador al inicio de la lista a ordenar y devuelve el apuntador al inicio de la lista ordenada.
 * Utiliza recursividad: divide la lista en dos mitades, las ordena recursivamente y luego las fusiona.
 * Complejidad: O(n log n) en tiempo; O(log n) en espacio (por la recursividad).
 */
Node* mergeSortList(Node* start) {
    if(!start || !start->next) {
        // 0 o 1 elemento: ya está ordenado
        return start;
    }
    // 1. Encontrar el punto medio para dividir la lista
    Node* slow = start;
    Node* fast = start;
    while(fast && fast->next) {
        fast = fast->next->next;
        if(fast) slow = slow->next;
    }
    Node* secondHalf = slow->next;
    slow->next = nullptr;         // cortar la lista en dos mitades
    if(secondHalf) secondHalf->prev = nullptr;
    // 2. Ordenar recursivamente las dos mitades
    Node* leftSorted = mergeSortList(start);
    Node* rightSorted = mergeSortList(secondHalf);
    // 3. Fusionar las dos mitades ordenadas
    return mergeSortedLists(leftSorted, rightSorted);
}

/*
 * 2.9 lowerBoundIP (búsqueda lineal)
 * Encuentra el primer nodo en la lista (desde 'start') cuya dirección IP numérica >= key.
 * (Convierte la IP del nodo a un valor de 32 bits para la comparación, o compara octeto a octeto).
 * Complejidad: O(n) en el peor caso, ya que recorre la lista secuencialmente.
 */
Node* lowerBoundIP(Node* start, unsigned long long key) {
    Node* ptr = start;
    while(ptr) {
        // Convertir IP del nodo actual a un número de 32 bits para comparar
        unsigned long long ipVal = ((unsigned long long)ptr->data.ip1 << 24) |
                                    ((unsigned long long)ptr->data.ip2 << 16) |
                                    ((unsigned long long)ptr->data.ip3 << 8)  |
                                    (unsigned long long)ptr->data.ip4;
        if(ipVal >= key) break;
        ptr = ptr->next;
    }
    return ptr;
}

/*
 * 2.10 upperBoundIP (búsqueda lineal)
 * Encuentra el primer nodo en la lista (desde 'start') cuya dirección IP numérica > key.
 * Devuelve nullptr si ningún nodo cumple (es decir, todos los nodos restantes tienen IP <= key).
 * Complejidad: O(n) en el peor caso.
 */
Node* upperBoundIP(Node* start, unsigned long long key) {
    Node* ptr = start;
    while(ptr) {
        unsigned long long ipVal = ((unsigned long long)ptr->data.ip1 << 24) |
                                    ((unsigned long long)ptr->data.ip2 << 16) |
                                    ((unsigned long long)ptr->data.ip3 << 8)  |
                                    (unsigned long long)ptr->data.ip4;
        if(ipVal > key) break;
        ptr = ptr->next;
    }
    return ptr;
}

/* ---------------- 3. FUNCIÓN PRINCIPAL (main) ---------------- */
int main() {
    Node* head = nullptr;
    Node* tail = nullptr;
    // 3.1 Lectura del archivo bitácora y almacenamiento en la lista
    ifstream theFile("bitacora.txt");
    if(!theFile.is_open()) {
        cerr << "Error: no se pudo abrir el archivo bitacora.txt\n";
        return 1;
    }
    string line;
    while(getline(theFile, line)) {
        entry E;
        size_t pos = 0;
        // Extraer tokens principales de la línea
        string month_str = tokenizer(line, pos);    // p.ej. "Jul"
        string day_str   = tokenizer(line, pos);    // p.ej. "18"
        string time_str  = tokenizer(line, pos);    // p.ej. "07:53:22"
        string ipPort    = tokenizer(line, pos);    // p.ej. "235.99.27.158:6526"
        string reason    = line.substr(pos);        // el resto de la línea es el mensaje de error
        // Llenar los campos de la estructura entry
        E.month  = months_int(month_str);
        E.day    = stoi(day_str);
        E.hour   = stoi(time_str.substr(0, 2));
        E.min    = stoi(time_str.substr(3, 2));
        E.sec    = stoi(time_str.substr(6, 2));
        E.totalTime = total_time(E.month, E.day, E.hour, E.min, E.sec);
        splitIp(ipPort, E.ip1, E.ip2, E.ip3, E.ip4, E.port);
        E.reason = reason;
        E.originLine = line;
        // Insertar el nuevo registro al final de la lista ligada
        Node* newNode = new Node(E);
        if(head == nullptr) {
            head = newNode;
            tail = newNode;
        } else {
            tail->next = newNode;
            newNode->prev = tail;
            tail = newNode;
        }
    }
    theFile.close();

    // 3.2 Ordenamiento de la lista por IP (ascendente) usando Merge Sort
    head = mergeSortList(head);
    // Actualizar el apuntador 'tail' después del ordenamiento (mover al último nodo)
    tail = head;
    if(tail) {
        while(tail->next) {
            tail = tail->next;
        }
    }

    // 3.3 Guardar la lista ordenada completa en el archivo "SortedData.txt"
    ofstream outFile("SortedData.txt");
    Node* it = head;
    while(it) {
        outFile << it->data.originLine;
        if(it->next) outFile << "\n";  // agregar newline si no es el último
        it = it->next;
    }
    outFile.close();

    // 3.4 Lectura de rango de IPs desde entrada estándar
    string startIP, endIP;
    if(!(cin >> startIP)) return 0;   // si no hay entrada, terminar
    if(!(cin >> endIP)) return 0;
    // Convertir IPs de entrada a valor numérico de 32 bits para facilidad de comparación
    int a1,b1,c1,d1;
    int a2,b2,c2,d2;
    int dummyPort;
    splitIp(startIP, a1, b1, c1, d1, dummyPort = 0);  // reuse splitIp by ignoring port (no port in input IP)
    splitIp(endIP,   a2, b2, c2, d2, dummyPort = 0);
    unsigned long long startKey = ((unsigned long long)a1<<24) | ((unsigned long long)b1<<16) | ((unsigned long long)c1<<8) | (unsigned long long)d1;
    unsigned long long endKey   = ((unsigned long long)a2<<24) | ((unsigned long long)b2<<16) | ((unsigned long long)c2<<8) | (unsigned long long)d2;
    if(startKey > endKey) {
        // si el rango está invertido, intercambiar
        unsigned long long temp = startKey;
        startKey = endKey;
        endKey = temp;
    }

    // 3.5 Búsqueda de los nodos de inicio y fin del rango en la lista ordenada
    Node* startNode = lowerBoundIP(head, startKey);
    Node* endBound  = upperBoundIP((startNode ? startNode : head), endKey);
    Node* endNode;
    if(endBound == nullptr) {
        // Si no hay nodo con IP > endKey, entonces endNode es el último con IP <= endKey (tail)
        endNode = tail;
    } else {
        endNode = endBound->prev;
    }

    // 3.6 Desplegar los registros en el rango [startIP, endIP] en orden **descendente** por IP
    if(startNode == nullptr || endNode == nullptr) {
        // Si no hay nodos en el rango, no imprimir nada (rango fuera de los datos)
        return 0;
    }
    // Comenzar desde endNode y moverse hacia atrás hasta startNode
    Node* cur = endNode;
    while(cur) {
        cout << cur->data.originLine << "\n";
        if(cur == startNode) break;
        cur = cur->prev;
    }

    return 0;
}
