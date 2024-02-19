# README 
# MARIN MARIUS DANIEL 332CC
## [Tema1 - APD](https://gitlab.cs.pub.ro/apd/tema1)

### Ideea de baza
* crearea unei structuri ce inglobeaza argumentele
necesare thread urilor si transformarea functiilor
secventiale intr-o singura functie in care vor fi
rulate toate thread urile

### Implementare:
* s-au paralelizat urmatoarele 3 functii:
1. `rescale_image`
2. `sample_grid`
3. `march`

### Pentru
* rescale_image
    - s-a impartit prima iteratie in intervale pentru fiecare thread
    - se asteapta toate thread urile sa termine executia pentru a nu crea un race condition atunci cand reactualizam imaginea

* sample_grid
    - se impart in intervale pentru fiecare thread iteratiile

* march
    - se imparte iteratia in intervale pentru fiecare thread

* thread uri
    - s-au creat 2 structuri (una care contine informatiile 
necesare si independente de thread) si una care contine
informatiile caracteristice fiecarui thread(id) + cealalta
    - cu ajutorul ei se transmite argumentul fiecarui thread

### Mai multe detalii se regasesc in cod.
