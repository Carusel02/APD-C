README tema1APD

Implementare:
-> s au paralelizat urmatoarele 3 functii:
* rescale_image
* sample_grid
* march

-> pentru rescale_image
* s a impartit prima iteratie in intervale pentru fiecare thread
* se asteapta toate thread urile sa termine executia pentru a nu
crea un race condition atunci cand reactualizam imaginea

-> pentru sample_grid
* se impart in intervale pentru fiecare thread iteratiile

-> pentru march
* se imparte iteratia in intervale pentru fiecare thread

-> pentru thread uri
* s au creat 2 structuri (una care contine informatiile 
necesare si independente de thread) si una care contine
informatiile caracteristice fiecarui thread(id) + cealalta
* cu ajutorul ei se transmite argumentul fiecarui thread