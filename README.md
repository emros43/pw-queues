```Zadanie polega na wykonaniu kilku implementacji kolejki nie-blokującej z wieloma czytelnikami i pisarzami: SimpleQueue, RingsQueue, LLQueue, BLQueue. Dwie implementacje polegają na użyciu zwykłych mutexów, a dwie kolejne na użyciu atomowych operacji, w tym kluczowego compare_exchange.

W każdym przypadku implementacja składa się ze struktury <kolejka> oraz metod:

    <kolejka>* <kolejka>_new(void) - alokuje (malloc) i inicjalizuję nową kolejkę.
    void <kolejka>_delete(<kolejka>* queue) - zwalnia wszelką pamięć zaalokowaną przez metody kolejki.
    void <kolejka>_push(<kolejka>* queue, Value value) - dodaje wartość na koniec kolejki.
    Value <kolejka>_pop(<kolejka>* queue) - pobiera wartość z początku kolejki albo zwraca EMPTY_VALUE jeśli kolejka jest pusta.
    bool <kolejka>_is_empty(<kolejka>* queue) - sprawdza czy kolejka jest pusta.

(Przykładowo pierwsza implementacja powinna definiować strukturę SimpleQueue oraz metody SimpleQueue* SimpleQueue_new(void) itp.)

Wartości w kolejce mają typ Value równy int64_t (dla wygodnego testowania, normalnie trzymalibyśmy tam raczej void*).
Użytkownicy kolejki nie używają wartości EMPTY_VALUE=0 ani TAKEN_VALUE=-1 (można je wykorzystać jako specjalne).
Użytkownicy kolejki gwarantują, że wykonują new/delete dokładnie raz, odpowiednio przed/po wszystkich operacjach push/pop/is_empty ze wszystkich wątków.

Wszystkie implementacje powinny bezpiecznie zachowywać się tak jakby operacje push, pop, is_empty były niepodzielne (patrząc na wartości zwracane użytkownikom kolejki).
Używanie kolejki nie może prowadzić do wycieków pamięci ani zakleszczeń.
Implementacje nie muszą w pełni gwarantować sprawiedliwości i braku zagłodzenia każdego wątku z osobna. Dozwolone jest np. by jeden wątek wykonujący push został zagłodzony, o ile innym wątkom udaje się zakończyć pushe.

Implementacje kolejek lock-free (LLQueue i BLQueue) powinny gwarantować, że w każdym równoległym wykonaniu operacji,
co najmniej jedna operacja push zakończy się w skończonej liczbie kroków (ograniczonej przez stałą zależną od liczby wątków i rozmiarów buforów w BLQueue),
co najmniej jedna operacja pop zakończy się w skończonej liczbie kroków, oraz
co najmniej jedna operacja is_empty zakończy się w skończonej liczbie kroków, jeśli takie operacje się zaczęły i nie zostały wstrzymane.
Gwarancja ta powinna zachodzić nawet jeśli inne wątki są w losowym momencie wstrzymane na dłuższy (np. przez wywłaszczenie).

Rozwiązania będą sprawdzane także pod kątem wydajności (ale odchylenia rzędu 10% od wzorcowego rozwiązania nie wpłyną na ocenę).
SimpleQueue: kolejka z listy jednokierunkowej, z dwoma mutexami

To jedna z prostszych implementacji kolejki. Osobny mutex dla producentów i dla konsumentów pozwala lepiej zrównoleglić operacje.

Struktura SimpleQueue składa się z:

    listy jednokierunkowej węzłów, gdzie węzeł zawiera:
        atomowy wskaźnik next na następny węzeł w liście,
        wartość typu Value;
    wskaźnika head na pierwszy węzeł w liście, wraz z mutexem do zabezpieczania dostępu do niego;
    wskaźnika tail na ostatni węzeł w liście, wraz z mutexem do zabezpieczania dostępu do niego.

Całość implementacji SimpleQueue jest częścią zadania, natomiast sugerujemy by kolejka zawsze zawierała pod head jeden węzeł z pustą/nieistotną wartością (aby uprościć obsługę przypadków brzegowych).
RingsQueue: kolejka z listy buforów cyklicznych

To połączenie SimpleQueue z kolejką zaimplementowaną na buforze cyklicznym, łączące nieograniczony rozmiar pierwszej z wydajnością drugiej (listy jednokierunkowe są stosunkowo powolne ze względu na ciągłe alokacje pamięci).

Struktura RingsQueue składa się z:

    listy jednokierunkowej węzłów, gdzie węzeł zawiera:
        atomowy wskaźnik next na następny węzeł w liście,
        bufor cykliczny w postaci tabeli RING_SIZE wartości typu Value,
        atomowy licznik push_idx wykonanych w tym węźle push-ów,
        atomowy licznik pop_idx wykonanych w tym węźle pop-ów.
    wskaźnika head na pierwszy węzeł w liście;
    wskaźnika tail na ostatni węzeł w liście (head i tail mogą wskazywać na ten sam węzeł);
    mutex pop_mtx do zamykania na czas całej operacji pop;
    mutex push_mtx do zamykania na czas całej operacji push.

Można założyć, że wszystkich operacji push i pop będzie co najwyżej 2^60.
Stała RING_SIZE jest zdefiniowana w RingsQueue.h i wynosi 1024, ale może zostać w testach zmieniona na mniejszą potęgę dwójki większą niż 2.
Operacji push i pop/is_empty powinny móc się odbywać równolegle, tzn. wstrzymanie wątku wykonującego push nie powinno blokować wątku wykonującego pop/is_empty, zaś wstrzymanie wątku wykonującego pop/is_empty nie powinno blokować wątku wykonującego push.
Kolejka RingsQueue będzie testowana głównie dla przypadku jednego wątku producenta (robiącego tylko operacje push) i jednego wątku konsumenta (robiącego tylko operacje pop i is_empty); w tym wypadku mutexy są zbędne.

Push powinien działać następująco:

    jeśli węzeł wskazany przez tail nie jest pełny, dodaje do jego bufora cyklicznego nową wartość, zwiększając przy tym push_idx.
    jeśli zaś jest pełny, to tworzy nowy węzeł i aktualizuje odpowiednie wskaźniki.

Pop powinien działać następująco:

    jeśli węzeł wskazany przez head nie jest pusty, to zwraca wartość z jego bufora cyklicznego, zwiększając przy tym pop_idx.
    jeśli węzeł ten jest pusty i nie ma następnika: zwraca EMPTY_VALUE.
    jeśli węzeł ten jest pusty i ma następnika: aktualizuje head na następny węzeł i zwraca wartość z jego bufora cyklicznego.

Uściślenie powyższych opisów jest częścią zadania, w szczególności należy przemyśleć kolejność wykonywanych sprawdzeń.
LLQueue: kolejka lock-free z listy jednokierunkowej

To jedna z najprostszych implementacji kolejki lock-free.

Struktura LLQueue składa się z:

    listy jednokierunkowej węzłów, gdzie węzeł zawiera:
        atomowy wskaźnik next na następny węzeł w liście,
        wartość typu Value, równa EMPTY_VALUE jeśli wartość z węzła została już pobrana;
    atomowego wskaźnika head na pierwszy węzeł w liście;
    atomowego wskaźnika tail na ostatni węzeł w liście;
    struktury HazardPointer (patrz niżej).

Push powinien działać w pętli próbując wykonać następujące kroki:

    Odczytujemy wskaźnik na ostatni węzeł kolejki (my tzn. wątek wykonujący push).
    Zamieniamy następnika na nowy węzeł z naszym elementem.
    3a. Jeśli się udało, aktualizujemy wskaźnik na ostatni węzeł w kolejce na nasz nowy węzeł i wychodzimy z funkcji.
    3b. Jeśli się nie udało (inny wątek zdążył już przedłużyć listę), to próbujemy wszystko od nowa, upewniając się że ostatni węzeł został zaktualizowany.

Pop powinien działać w pętli próbując wykonać następujące kroki:

    Odczytujemy wskaźnik na pierwszy węzeł kolejki.
    Odczytujemy wartość z tego węzła i zamieniamy ją na EMPTY_VALUE.
    3a. Jeśli odczytana wartość była inna niż EMPTY_VALUE, to aktualizujemy wskaźnik na pierwszy węzeł (jeśli potrzeba) i zwracamy wynik.
    3b. Jeśli odczytana wartość była EMPTY_VALUE, to sprawdzamy czy kolejka jest pusta.
    Jeśli tak, zwracamy EMPTY_VALUE, a jeśli nie, to próbujemy wszystko od nowa, upewniając się że pierwszy węzeł został zaktualizowany.

Uściślenie powyższych algorytmów jest częścią zadania (natomiast podana kolejność numerowanych punktów jest prawidłowa).
Całość implementacji operacji LLQueue_is_empty jest częścią zadania.
BLQueue: kolejka lock-free z listy buforów

To jedna z prostszych a bardzo wydajnych implementacji kolejki. Idea tej kolejki to połączenie rozwiązania z listą jednokierunkową z rozwiązaniem gdzie kolejka jest prostą tablicą z atomowymi indeksami do wstawiania i pobierania elementów (ale liczba operacji byłaby ograniczona przez długość tablicy). Łączymy zalety obu robiąc listę tablic; do nowego węzła listy potrzebujemy przejść dopiero kiedy tablica się zapełniła. Tablica nie jest jednak tutaj buforem cyklicznym, każde pole w niej jest zapełnione co najwyżej raz (wariant z buforami cyklicznymi byłby dużo trudniejszy).

Struktura BLQueue składa się z:

    listy jednokierunkowej węzłów, gdzie węzeł zawiera:
        atomowy wskaźnik next na następny węzeł w liście,
        bufor z BUFFER_SIZE atomowych wartości typu Value,
        atomowy indeks push_idx następnego miejsca w buforze do wypełnienia przez push (rośnie przy każdej próbie push),
        atomowy indeks pop_idx następnego miejsca w buforze do opróżnienia przez pop (rośnie przy każdej próbie pop);
    atomowego wskaźnika head na pierwszy węzeł w liście;
    atomowego wskaźnika tail na ostatni węzeł w liście;
    struktury HazardPointer (patrz niżej).

Kolejka początkowo zawiera jeden węzeł z pustym buforem. Elementy bufora początkowo mają wartość EMPTY_VALUE.
Operacje pop będą zamieniać pobrane lub puste wartości na wartość TAKEN_VALUE (pozwalamy na to, że pop czasem zmarnuje w ten sposób elementy tablicy).
Stała BUFFER_SIZE jest zdefiniowana w BLQueue.h i wynosi 1024, ale może zostać w testach zmieniona na mniejszą potęgę dwójki większą niż 2.

Push powinien działać w pętli próbując wykonać następujące kroki:

    Odczytujemy wskaźnik na ostatni węzeł kolejki.
    Pobieramy i inkrementujemy z tego węzła indeks miejsca w buforze do wypełnienia przez push (nikt inny nie będzie już próbował push-ować w to miejsce).

3a. Jeśli indeks jest mniejszy niż rozmiar bufora, próbujemy wstawić element w to miejsce bufora.

    Jeśli inny wątek zdążył zmienić to miejsce (inny wątek wykonujący pop mógł je zmienić na TAKEN_VALUE), to próbujemy wszystko od nowa.
    A jeśli nam udało się wstawić element, wychodzimy z funkcji.

3b. Jeśli zaś indeks jest większy-równy niż rozmiar bufora, to znaczy, że bufor jest pełny i będziemy musieli utworzyć lub przejść do następnego węzła. W tym celu najpierw sprawdzamy, czy został już utworzony następny węzeł.
4a. Jeśli tak, to upewniamy się że wskaźnik na ostatni węzeł w kolejce się zmienił i próbujemy wszystko od nowa.
4b. Jeśli nie, to tworzymy nowy węzeł, od razu z naszym jednym elementem w buforze. Próbujemy wskaźnik do nowego węzła wstawić jako następnik.

    Jeśli się nie udało (inny wątek zdążył już przedłużyć listę), to usuwamy nasz węzeł i próbujemy wszystko od nowa.
    Jeśli się udało, to aktualizujemy wskaźnik na ostatni węzeł w kolejce na nasz nowy węzeł.

Pop powinien działać w pętli próbując wykonać następujące kroki:
1. Odczytujemy wskaźnik na pierwszy węzeł kolejki.
2. Pobieramy i inkrementujemy z tego węzła indeks miejsca w buforze do odczytania przez pop (nikt inny nie będzie już próbował pop-ować z tego miejsca).
3a. Jeśli indeks jest mniejszy niż rozmiar bufora, odczytujemy element z tego miejsca bufora i podmieniamy na TAKEN_VALUE.

    Jeśli pobraliśmy EMPTY_VALUE, to próbujemy wszystko od nowa.
    Jeśli pobraliśmy inny element, wychodzimy z funkcji.

3b. Jeśli zaś indeks jest większy-równy niż rozmiar bufora, to znaczy, że bufor jest całkowicie opróżniony i będziemy musieli przejść do następnego węzła. W tym celu najpierw sprawdzamy, czy został już utworzony następny węzeł.
4a. Jeśli nie, to kolejka jest pusta, wychodzimy z funkcji.

4b. Jeśli tak, to upewniamy się że wskaźnik na pierwszy węzeł w kolejce się zmienił i próbujemy wszystko od nowa.

Uściślenie powyższych algotytmów jest częścią zadania (natomiast podana kolejność numerowanych punktów jest prawidłowa).
Całość implementacji operacji BLQueue_is_empty jest częścią zadania.

Zauważmy, że o ile push-e i pop-y mogą długo w kółko próbować wszystko od nowa w 3a, to po co najwyżej BUFFER_SIZE iteracjach któryś powiększy indeks (odpowiednio push_idx lub pop_idx) tak, że przejdzie do 3b, a tam może restartować tylko jeśli innej operacji się powiedzie. Dlatego powyższy algorytm jest lock-free. Algorytm ma pewne wady gdy kolejka jest często pusta, ale dla uproszczenia nie rozważamy w tym zadaniu poprawiania wydajności tego przypadku. Pop może marnować miejsca w kolejce (zamieniając EMPTY_VALUE na TAKEN_VALUE), ale dzięki temu w najczęstszym przypadku pop wykonuje tylko inkrementację i pobranie.
Hazard Pointer

Hazar Pointer to technika do radzenia sobie z problemem bezpiecznego zwalniania pamięci w strukturach danych współdzielonych przez wiele wątków oraz z problemem ABA. Idea jest taka, że każdy wątek może zarezerwować sobie jeden adres na węzeł (jeden na każdą instację kolejki), który potrzebuje zabezpieczyć przed usunięciem (lub podmianą ABA) na czas operacji push/pop/is_empty. Chcąc zwolnić węzeł (free()), wątek zamiast tego dodaje jego adres na swój zbiór wycofanych adresów i później okresowo przegląda ten zbiór, zwalniając adresy, które nie są zarezerwowane.

Częściowe rozwiązania chcące pominąć Hazard Pointer powinny zamienić zwalnianie węzła LLQueue i BLQueue przez free() na HazardPointer_retire() z pustą implementacją tej funkcji: tzn. lepiej mieć wyciek pamięci niż naruszenie ochrony pamięci (przez użycie zwolnionej pamięci, którego trudno uniknąć w tych kolejkach inaczej niż implementując Hazard Pointer).

Struktury HazardPointer należy użyć w implementacjach operacji push/pop/is_empty kolejek LLQueue i BLQueue, zabezpieczając adres, spod którego będziemy odczytywać wartości (w opisanych implementacjach w każdym momencie jest co najwyżej jeden taki adres na wątek) i zdejmując zabezpieczenie przed wyjściem z operacji. Zamiast zwalniać węzeł, należy go wycofać, jeśli inny wątek może do niego jeszcze zaglądać.

Struktura HazardPointer powinna składać się z:

    tablicy zawierającej atomowy wskaźnik dla każdego wątku – zawierający "zarezerwowany" adres węzła przez dany wątek;
    tablicy zbiorów wskaźników dla każdego wątku – adresy "wycofane" (retired), czyli do późniejszego zwolnienia;

Implementacja składa się z następujących metod:

    void HazardPointer_register(int thread_id, int num_threads) – rejestruje wątek o identyfikatorze thread_id.
    void HazardPointer_initialize(HazardPointer* hp) – inicjalizuje (zaalokowaną już) strukturę: zarezerwowane adresy są wszystkie NULL.
    void HazardPointer_finalize(HazardPointer* hp) – czyście wszelkie rezerwacje, zwalnia pamięć zaalokowaną przez metody struktury oraz zwalnia wszystkie adresy z tablicy wycofanych (nie zwalnia samej struktury HazardPointer).
    void* HazardPointer_protect(HazardPointer* hp, const AtomicPtr* atom) – zapisuje w tablicy zarezerwowanych adresów adres odczytany z atom pod indeksem thread_id i zwraca go (nadpisuje istniejącą rezerwację, jeśli taka była dla thread_id).
    void HazardPointer_clear(HazardPointer* hp) – usuwa rezerwację, tzn. zamienia adres pod indeksem thread_id na NULL.
    void HazardPointer_retire(HazardPointer* hp, void* ptr) – dodaje ptr do zbioru adresów wycofanych, za których zwolnienie odpowiedzialny jest wątek thread_id. Następnie jeśli rozmiar zbióru wycofanych przekracza próg zdefiniowany stałą RETIRED_THRESHOLD (równą np. MAX_THREADS), to przegląda wszystkie adresy w swoim zbiorze i zwalnia (free()) te, które nie są zarezerwowane przez żaden wątek (usuwając je też ze zbioru).

Użytkownicy kolejek używających HazardPointer gwarantują, że każdy wątek wywoła HazardPointer_register z unikalnym thread_id (liczba całkowita z przedziału [0, num_threads)) przed wykonaniem jakiejkolwiek operacji push/pop/is_empty na kolejce, z tym samym num_threads dla wszystkich wątków. Funkcja <kolejka>_new (a więc także HazardPointer_initialize) może zostać przez użytkowników wywołana przed HazardPointer_register. Implementacja HazardPointer_register powinna zapisać liczbę wątków i numer wątku w globalnej zmiennej thread_local int _thread_id = -1; i używać jej w metodach HazardPointer.

Uwaga: w implementacji HazardPointer_protect należy odczytać adres pod atom wielokrotnie, aby upewnić się że nie został wycofany i zwolniony w czasie między odczytem a zarezerwowaniem. Można przy tym założyć, że adresy węzłów są zwalniane tylko przez HazardPointer_retire i następuje to zawsze po zmianie wszystkich atomowych wskaźników (w szczególności wartości w atom) na inny adres.

Można założyć, że num_threads <= MAX_THREADS, gdzie MAX_THREAD = 128. Struktura przy pustych zbiorach może zajmować O(MAX_THREAD^2) pamięci, może też alokować ją statycznie dla najgorszego przypadku, tzn. sizeof(HazardPointer) może siegać 10 * 128 * 128 * 64 bitów pamięci niezależnie od podanej wartości num_threads. Natomiast implementacja HazardPointer_retire powinna używać prawdziwej wartości num_threads aby nie przeglądać niepotrzebnie wielu elementów.
Wymagania techniczne

    Implementacja powinna być w języku C, zgodna ze standardem C11 lub nowszym.

    Rozwiązanie należy wysłać na Moodle w postaci pliku ab1234567.tar (lub .tgz), zawierającego folder ab1234567, zawierającego CMakeLists.txt. Oczywiście ab1234567 należy zmienić na własne inicjały i numer indeksu.

    Wolno modyfikować jedynie pliki .c oraz plik HazardPointer.h. Dodatkowe pliki (np. do testów) mogą istnieć, ale zostaną zignorowane (usunięte przed kompilacją). Plik CMakeLists.txt przy testowaniu zostanie nadpisany załączonym.

    Rozwiązanie powinno się kompilować na maszynie students (poleceniem mkdir build && cd build && cmake .. && make).

    Rozwiązanie będzie kompilowane i wykonywane jedynie na platformie takiej jak students, tzn. współczesny Linux z gcc i architekturą x86_64; w szczególności dostępne są 64-bitowe lock-free atomowe liczby całkowite i wskaźniki; nie ma różnicy między atomic_compare_exchange_weak i atomic_compare_exchange_strong (sugerujemy w tym zadaniu wszędzie używać strong dla uproszczenia, to gwarantuje że użycie na innych platformach byłoby bezpieczne, choć potencjalnie mniej wydajne).

    Użycie clang-format i clang-tidy nie jest wymagane (ale jest jak zwykle zalecane).

    Nie wolno używać bibliotek innych niż standardowe i pthreads.

    Nie wolno używać asemblera.

    Implementacje nie mogą tworzyć własnych wątków ani procesów.

    W implementacji SimpleQueue i RingsQueue z biblioteki pthreads wolno używać jedynie funkcji pthread_mutex_init, _destroy, _lock, _unlock.

    W implementacji SimpleQueue nie wolno używać funkcji atomowych innych niż atomic_init, atomic_load, atomic_store.

    W implementacji LLQueue i BLQueue nie wolno używać biblioteki pthreads.

    Nie wolno używać zwykłego odczytu i operatorów na typach atomowych (przypisania, inkrementacji, itp.), zamiast tego należy explicite użyć funkcji atomic_load, atomic_store, itp. (jest to ważne dla działania testów).

    Z funkcji z biblioteki <stdatomic.h> wolno używać jedynie: atomic_init, atomic_load, atomic_store, atomic_exchange, atomic_compare_exchange_weak, atomic_compare_exchange_strong, atomic_fetch_add, atomic_fetch_sub.

```