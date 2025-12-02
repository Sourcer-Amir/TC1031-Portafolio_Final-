/*
    Descripción: Programa que lee un archivo de bitácora, almacena los registros agrupados
    por dirección IP, cuenta la frecuencia de accesos de cada IP y despliega las 5 IPs con
    mayor cantidad de accesos en orden descendente, mostrando toda su información en el
    formato original del archivo bitacora.txt.

    [Ayleen Osnaya Ortega] - [A01426008]
    [José Luis Gutiérrez Quintero] - [A01739337]
    [Santiago Amir Rodríguez González] - [A01739942]
    Fecha: 28/10/2025
*/

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
using namespace std;

/* ---------------- 1. ESTRUCTURA PRINCIPAL ----------------
 * Representa un registro de bitácora.
 * entry: campos parseados de una línea (fecha, hora, IP, puerto, motivo).
 * Se almacena también la línea original completa para impresión exacta.
 */
struct entry {
    int month, day, hour, min, sec;    // Fecha y hora desglosada
    long long totalTime;              // Clave numérica de fecha/hora para comparar fácilmente
    int ip1, ip2, ip3, ip4;           // Octetos de la IP
    int port;                        // Puerto de la conexión
    string reason;                   // Mensaje de error o descripción
    string originLine;               // Línea original completa (útil para imprimir exactamente igual)
};

/* ---------------- 2. ESTRUCTURA PARA CLAVE DE IP ----------------
 * Representa una dirección IP única (sin considerar puerto).
 * Se utiliza como clave en el map para agrupar todos los accesos de la misma IP.
 */
struct IPKey {
    int ip1, ip2, ip3, ip4;
    
    /*
     * Operador de comparación necesario para usar IPKey como clave en map.
     * Compara las IPs octeto por octeto en orden numérico.
     * Complejidad: O(1)
     */
    bool operator<(const IPKey& other) const {
        if(ip1 != other.ip1) return ip1 < other.ip1;
        if(ip2 != other.ip2) return ip2 < other.ip2;
        if(ip3 != other.ip3) return ip3 < other.ip3;
        return ip4 < other.ip4;
    }
};

/* ---------------- 3. ESTRUCTURA PARA DATOS AGRUPADOS POR IP ----------------
 * Almacena toda la información relacionada con una IP específica:
 * - La clave de la IP (ip1, ip2, ip3, ip4)
 * - Vector con todas las entradas (registros de acceso) de esa IP
 * - Contador de cuántos accesos tiene esa IP
 */
struct IPData {
    IPKey key;
    vector<entry> entries;  // Todas las entradas de esta IP
    int count;             // Número total de accesos de esta IP
};

/* ---------------- 4. FUNCIONES AUXILIARES ---------------- */

/*
 * 4.1 months_int
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
 * 4.2 tokenizer
 * Extrae el siguiente token (secuencia de caracteres hasta el siguiente espacio) de la línea dada,
 * a partir de la posición pos. Actualiza pos a la posición después del token extraído.
 * Si no se encuentra más espacios, devuelve el resto de la línea.
 * Complejidad: O(n) en el peor caso (n = longitud restante de la línea).
 */
string tokenizer(const string &s, size_t &pos) {
    if(pos >= s.size()) return "";
    size_t start = pos;
    size_t found = s.find(' ', pos);
    if(found == string::npos) {
        pos = s.size();
        return s.substr(start);
    } else {
        pos = found + 1;
        return s.substr(start, found - start);
    }
}

/*
 * 4.3 splitIp
 * Divide una cadena "IP:PORT" en sus componentes numéricos.
 * Parámetros de salida: a,b,c,d corresponden a los 4 octetos de la IP, y p al puerto.
 * Complejidad: O(k), donde k es la longitud de la cadena IP:PORT (muy pequeña, k < 20 caracteres típicamente).
 */
void splitIp(const string &ipPort, int &a, int &b, int &c, int &d, int &p) {
    string ipStr;
    size_t colon = ipPort.find(':');
    if (colon == string::npos) {
        ipStr = ipPort;
        p = 0;
    } else {
        ipStr = ipPort.substr(0, colon);
        string portStr = ipPort.substr(colon + 1);
        p = portStr.empty() ? 0 : stoi(portStr);
    }

    size_t pos = 0, next;
    next = ipStr.find('.', pos);
    a = stoi(ipStr.substr(pos, next - pos));
    pos = (next == string::npos) ? ipStr.size() : next + 1;
    
    next = ipStr.find('.', pos);
    b = stoi(ipStr.substr(pos, next - pos));
    pos = (next == string::npos) ? ipStr.size() : next + 1;
    
    next = ipStr.find('.', pos);
    c = stoi(ipStr.substr(pos, next - pos));
    pos = (next == string::npos) ? ipStr.size() : next + 1;
    
    d = stoi(ipStr.substr(pos));
}

/*
 * 4.4 total_time
 * Calcula una clave numérica a partir de una fecha y hora desglosada.
 * Sirve para comparar rápidamente dos fechas/horas.
 * Complejidad: O(1).
 */
long long total_time(int month, int day, int hour, int minute, int second) {
    return (((((long long)month * 31 + day) * 24 + hour) * 60 + minute) * 60 + second);
}

/*
 * 4.5 lessEntry
 * Comparador que define el orden cronológico para dos registros de la misma IP.
 * Criterios de ordenamiento (de mayor prioridad a menor):
 * 1) Fecha y hora (totalTime) como criterio principal.
 * 2) Mensaje de error (reason) como desempate final.
 * Devuelve true si 'a' debe ir antes que 'b' según este orden.
 * Complejidad: O(m) en el peor caso, donde m es la longitud de la cadena reason a comparar.
 */
bool lessEntry(const entry &a, const entry &b) {
    if(a.totalTime != b.totalTime) return a.totalTime < b.totalTime;
    return a.reason < b.reason;
}

/* ---------------- 5. FUNCIÓN PRINCIPAL (main) ---------------- */
int main() {
    /*
     * 5.1 Lectura del archivo bitácora y agrupación por IP
     * Utiliza un map<IPKey, vector<entry>> para agrupar todos los registros de cada IP.
     * La clave del map es la IP (sin puerto), y el valor es un vector con todos los
     * registros de acceso de esa IP.
     * Complejidad: O(n log m) donde n = número de líneas del archivo, m = número de IPs únicas.
     * El factor log m viene de las inserciones en el map (árbol rojo-negro).
     */
    map<IPKey, vector<entry>> ipMap;
    
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
        string month_str = tokenizer(line, pos);
        string day_str   = tokenizer(line, pos);
        string time_str  = tokenizer(line, pos);
        string ipPort    = tokenizer(line, pos);
        string reason    = line.substr(pos);
        
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
        
        // Agrupar por IP (sin considerar puerto como parte de la clave)
        IPKey key = {E.ip1, E.ip2, E.ip3, E.ip4};
        ipMap[key].push_back(E);
    }
    theFile.close();

    /*
     * 5.2 Creación de vector de IPData y ordenamiento interno por fecha/hora
     * Para cada IP en el map, creamos un objeto IPData que contiene:
     * - La clave de la IP
     * - Todas sus entradas ordenadas cronológicamente
     * - El conteo total de accesos
     * Complejidad: O(m * k log k) donde m = número de IPs únicas, k = promedio de accesos por IP.
     */
    vector<IPData> ipDataList;
    for(auto& pair : ipMap) {
        IPData data;
        data.key = pair.first;
        data.entries = pair.second;
        data.count = pair.second.size();
        
        // Ordenar las entradas de esta IP por fecha/hora (criterio de desempate de la especificación)
        sort(data.entries.begin(), data.entries.end(), lessEntry);
        
        ipDataList.push_back(data);
    }
    
    /*
     * 5.3 Ordenamiento por cantidad de accesos (descendente)
     * Ordena el vector de IPData por frecuencia de accesos de mayor a menor.
     * En caso de empate en la cantidad de accesos, desempata por valor numérico de IP (descendente).
     * Complejidad: O(m log m) donde m = número de IPs únicas.
     */
    sort(ipDataList.begin(), ipDataList.end(), 
         [](const IPData& a, const IPData& b) {
             // Criterio principal: mayor cantidad de accesos primero
             if(a.count != b.count) return a.count > b.count;
             // Desempate: IP con mayor valor numérico primero
             if(a.key.ip1 != b.key.ip1) return a.key.ip1 > b.key.ip1;
             if(a.key.ip2 != b.key.ip2) return a.key.ip2 > b.key.ip2;
             if(a.key.ip3 != b.key.ip3) return a.key.ip3 > b.key.ip3;
             return a.key.ip4 > b.key.ip4;
         });
    
    /*
     * 5.4 Despliegue de las 5 IPs con más accesos
     * Imprime todas las líneas originales de las 5 IPs que tienen mayor cantidad de accesos.
     * Cada línea se imprime exactamente como aparece en el archivo bitacora.txt original.
     * Complejidad: O(k) donde k = suma de accesos de las top 5 IPs (en el peor caso, O(n)).
     */
    int limit = min(5, (int)ipDataList.size());
    for(int i = 0; i < limit; i++) {
        // Imprimir todas las líneas de esta IP en formato original
        for(const auto& e : ipDataList[i].entries) {
            cout << e.originLine << "\n";
        }
    }

    return 0;
}

/*
* COMPLEJIDAD TOTAL:
 * 
 * Componentes principales del algoritmo:
 * 
 * 1. Lectura y agrupación por IP: O(n log m)
 *    - n líneas del archivo
 *    - m IPs únicas
 *    - Cada inserción en map cuesta O(log m)
 * 
 * 2. Ordenamiento interno por fecha/hora: O(m * k log k)
 *    - m IPs únicas
 *    - k accesos promedio por IP
 *    - Cada IP se ordena con sort: O(k log k)
 * 
 * 3. Ordenamiento por frecuencia: O(m log m)
 *    - m elementos en el vector ipDataList
 * 
 * 4. Impresión de resultados: O(k')
 *    - k' = total de líneas a imprimir (máximo 5 IPs)
 *    - En el peor caso: O(n) si las 5 IPs concentran todos los accesos
 * 
 * COMPLEJIDAD FINAL: O(n log m + m * k log k + m log m)
 * 
 * En el caso promedio donde k es constante o pequeño respecto a n:
 * - Complejidad simplificada: O(n log m)
 * 
 * En el peor caso donde pocas IPs concentran todos los accesos (k ≈ n/m):
 * - Complejidad: O(n log n)
 * 
 * COMPLEJIDAD ESPACIAL: O(n)
 * - Se almacenan todos los registros del archivo en memoria
 * - El map y los vectores requieren espacio proporcional al número de registros
 */