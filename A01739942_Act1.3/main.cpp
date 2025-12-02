/*
    Descripción: Programa que lee un archivo de bitácora, ordena las entradas por fecha/hora
    y permite buscar registros en un rango de fechas, además de guardar los resultados filtrados.

 *Autores:
 * [Ayleen Osnaya Ortega] - [A01426008]
 * [José Luis Gutiérrez Quintero] - [A01739337]
 * [Santiago Amir Rodríguez González] - [A01739942]
    Fecha: 21/09/2025
*/

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
using namespace std;


/* ---------------- 1. ESTRUCTURA PRINCIPAL ----------------
 * Representa un registro de bitácora.
 * Complejidad: O(1)
*/
struct entry{
    int month, day, hour, min, sec; // Fecha y hora desglosada
    long long totalTime;            // Clave numérica para ordenar por fecha/hora
    int ip1, ip2, ip3, ip4;         // Octetos de la IP (para comparar punto por punto)
    int port;                       // Puerto
    string reason;                  // Resto del mensaje (motivo / descripción)
    string originLine;              // Línea original tal cual (útil para imprimir)
};

// ---------------- 2. FUNCIONES AUXILIARES ----------------
/*
 * 2.1 months_int
 * Función que convierte el nombre del mes a un número (1-12).
 * Complejidad: O(1)
 */
int months_int(string& month){
    string months [12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    for(int i = 0; i < 12; i++)
        if(months[i] == month) 
            return i + 1;
    return -1; // devuelve -1 si el mes no es válido (no debería ocurrir si el archivo está bien formado)
}

/*
 * 2.2 tokenizer
 *Tokenizador sencillo: extrae el siguiente token separado por espacios
 * pos es la posición de inicio y se actualiza para la siguiente llamada
 * Complejidad: O(n) en el peor caso → depende de la longitud de la línea.
 */

string tokenizer(string& line, size_t &pos){
    size_t n = line.size();
    while (pos < n && line[pos] == ' ') 
        ++pos;
    if(pos >= n)
        return "";
    size_t start = pos;
    while (pos < n && line[pos] != ' ')
        ++pos;
    return line.substr(start, pos - start);
}

/*
 * 2.3 splitIp
 * Divide una cadena "ip:port" en sus componentes numéricos
 * Recibe el string i y llena a,b,c,d,p
 * Complejidad: O(1) 
 */

void splitIp(string &i, int &a, int &b, int &c, int &d, int &p){
    size_t colon = i.find(':'); // Buscar ':'
    p = stoi(i.substr(colon + 1)); // Puerto
    string ip = i.substr(0, colon); // IP
    int *oct = &a;
    size_t pos = 0;
    for (int k = 0; k < 3; ++k) {
        size_t next = ip.find('.', pos);
        oct[k] = stoi(ip.substr(pos, next - pos)); // oct[0]=a, oct[1]=b, oct[2]=c
        pos = next + 1;
    }
    d = stoi(ip.substr(pos)); // último octeto
}

/*
 * 2.5 TotalTime
 * Calcula una clave numérica a partir de fecha y hora (segundos relativos)
 * Usa 31 días por mes, conforme a la suposición del enunciado
 * Complejidad: O(1).
 */

long long total_time(int month, int day, int hour, int minute, int second){
    return (((((month * 31LL + day) * 24 + hour) * 60 + minute) * 60) + second);
} 

/* -------------------------------------------------------------
 * 2.6 lessEntry
 * Comparador que aplica el orden requerido:
 * 1) totalTime (fecha/hora)
 * 2) ip1, ip2, ip3, ip4 (octeto por octeto)
 * 3) port
 * 4) reason (cadena) como desempate final
 * complejidad: O(n).
  -------------------------------------------------------------*/
bool lessEntry(const entry &A, const entry &B) {
    if (A.totalTime != B.totalTime) 
        return A.totalTime < B.totalTime;
    if (A.ip1 != B.ip1) 
        return A.ip1 < B.ip1;
    if (A.ip2 != B.ip2) 
        return A.ip2 < B.ip2;
    if (A.ip3 != B.ip3) 
        return A.ip3 < B.ip3;
    if (A.ip4 != B.ip4) 
        return A.ip4 < B.ip4;
    if (A.port != B.port) 
        return A.port < B.port;
    return A.reason < B.reason;
}


// ---------------- 3. QUICK SORT ----------------
/*
 * Implementación del algoritmo QuickSort para ordenar las entradas.
 * Complejidad: O(n log n) en promedio, O(n^2) en el peor caso.
 */
 
/*-------------------------------------------------------------
 * 3.1 Swap 
 * simple para intercambiar dos entries (utilizado por quicksort)
 * complejidad: O(n).
  -------------------------------------------------------------*/
void swap(entry& a, entry& b) {
    entry temp = a;
    a = b;
    b = temp;
}

/* -------------------------------------------------------------
 * 3.2 Partición 
 * estilo Lomuto para quicksort sobre vector<entry>
 * Se usa lessEntry para comparar registros según la prioridad definida.
 * Devuelve índice del pivote
 * complejidad: O(n).
  -------------------------------------------------------------*/
int particion(vector<entry>& a, int low, int high) {
    entry pivot = a[high];
    int i = low - 1;
    for (int j = low; j < high; ++j) {
        if (lessEntry(a[j], pivot)) {
            ++i;
            swap(a[i], a[j]);
        }
    }
    swap(a[i + 1], a[high]);
    return i + 1;
}

/* -------------------------------------------------------------
 * 3.3 Quicksort
 * Quicksort recursivo usando particion de Lomuto
 * complejidad: O(n^2)
  -------------------------------------------------------------*/
void quickSort(vector<entry>& a, int low, int high) {
    if (low < high) {
        int p = particion(a, low, high);
        quickSort(a, low, p - 1);
        quickSort(a, p + 1, high);
    }
}

// ---------------- 4. BÚSQUEDAS ----------------

/* -------------------------------------------------------------
 * 4.1 lowerBoundSum
 * Búsquedas binarias para encontrar límites por totalTime
 * lowerBoundSum -> primera posición con totalTime >= thetime
 * upperBoundSum -> primera posición con totalTime > thetime
 * complejidad: O(log n).
  -------------------------------------------------------------*/
int lowerBoundSum(const vector<entry> &v, long long thetime) { 
    int l = 0, r = (int)v.size();
    while (l < r) {
        int m = l + (r - l) / 2;
        if (v[m].totalTime < thetime) l = m + 1; 
        else r = m;
    } 
    return l;
} //Binary search to find the lower bound 


/*
 * 4.2 upperBoundSum
 * Devuelve el índice del primer registro con totalTime > thetime
 * Complejidad: O(log n)
 */
 
int upperBoundSum(const vector<entry> &v, long long thetime) {
    int l = 0, r = (int)v.size();
    while (l < r) {
        int m = l + (r - l) / 2;
        if (v[m].totalTime <= thetime) l = m + 1; 
        else r = m;
    }
    return l;
} //Binary search to find the upper bound 


/* ---------------- 5. FUNCIÓN PRINCIPAL ---------------- 

/* -------------------------------------------------------------
 * Función principal
 * 1) Lee bitacora.txt línea por línea
 * 2) Parsea tokens (mes, día, hora, ip:port, razón)
 * 3) Calcula totalTime y divide la IP en octetos
 * 4) Inserta registros en logs
 * 5) Ordena con quickSort usando la comparación definida
 * 6) Escribe sorted.txt con las líneas ordenadas
 * 7) Lee rango de fechas desde stdin y muestra registros en ese rango
 * complejidad: O(n^2)
  -------------------------------------------------------------*/
int main(){
    ifstream theFile("bitacora.txt");
    vector<entry> logs;
    string line;

    // Lectura y parsing: asumimos que bitacora.txt está bien formado
    while(getline(theFile,line)){
        entry TO; // temporal para cada línea
        size_t pos = 0; // posición para tokenizer
        string month_str = tokenizer(line, pos);   // token mes (ej. "Feb")
        string day_str = tokenizer(line, pos);     // token día (ej. "01", "30")
        string time_str = tokenizer(line, pos);    // token hora (HH:MM:SS)
        string ipPort = tokenizer(line, pos);      // token ip:port
        string reason = line.substr(pos);          // resto de la línea -> reason

        TO.reason = reason;        
        TO.month  = months_int(month_str);
        TO.day = stoi(day_str);
        TO.hour = stoi(time_str.substr(0,2));
        TO.min = stoi(time_str.substr(3,2));
        TO.sec = stoi(time_str.substr(6,2));

        // clave/tiempo total para ordenar (segundos relativos)
        TO.totalTime = total_time(TO.month, TO.day, TO.hour,  TO.min, TO.sec);

        // dividir IP:PORT en sus componentes numéricos
        splitIp(ipPort, TO.ip1, TO.ip2, TO.ip3, TO.ip4, TO.port);

        TO.originLine = line;   // almacenamos la línea original tal cual
        logs.push_back(TO);     // agregamos al vector
    }
    theFile.close();

    // Ordenar los registros según la comparación definida
    quickSort(logs, 0, (int)logs.size() -1);

    // Escribir todos los registros ordenados en sorted.txt (misma estructura que la entrada)
    ofstream outFile("sorted.txt");
    for (size_t i = 0; i < logs.size(); i++) {
        outFile << logs[i].originLine;
        if (i < logs.size() - 1) {  // Solo añade una nueva línea si no es la última entrada.
         outFile << "\n";
        }
    }
    outFile.close(); 

    // Lectura de rango de fechas desde stdin (para pruebas automáticas)
    int sm, sd, em, ed;
    if (!(cin >> sm >> sd)) return 0;
    if (!(cin >> em >> ed)) return 0;

    // Convertir rango a totalTime (incluir desde 00:00:00 hasta 23:59:59)
    long long sk = total_time(sm, sd, 0, 0, 0);
    long long ek = total_time(em, ed, 23, 59, 59);
    if (sk > ek) { long long t = sk; sk = ek; ek = t; }

    // Encontrar índices con búsqueda binaria y mostrar los registros del rango
    int start = lowerBoundSum(logs, sk);
    int end = upperBoundSum(logs, ek) - 1;
    if (start < 0) 
        start = 0;
    if (end >= (int)logs.size()) 
        end = (int)logs.size() - 1;
    for (int i = start; i <= end; ++i) 
        cout << logs[i].originLine << '\n';

    return 0;
}