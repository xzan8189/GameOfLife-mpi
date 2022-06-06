#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "mpi.h"

#define ALIVE 'a'
#define DEAD 'd'
#define ALIVE_EMOJI "🦠"
#define DEAD_EMOJI "💀"
#define SOSTITUISCI_CON_DEAD_EMOJI "⬛"

/* DESCRIZIONE CODICE
? compili codice: mpicc GameOfLife_finale.c -o fin
! esempio di esecuzione: mpirun --allow-run-as-root -np 2 fin 5 5 2
!                        mpirun --allow-run-as-root -np 2 fin {ROW_SIZE} {COL_SIZE} {GENERATIONS}
*/

//Stampa matrice
void print_array_int(int arr[], int dim) {
    for (int i=0; i<dim; i++)
        printf("%d ", arr[i]);
    printf("\n");
}

void print_array_float(float arr[], int dim) {
    for (int i=0; i<dim; i++)
        printf("%.1f ", arr[i]);
    printf("\n");
}

void print_array_char(char arr[], int dim) {
    for (int i=0; i<dim; i++)
        printf("%c ", arr[i]);
    printf("\n");
}

void printMatrixEmoji(char *matrix, int ROW_SIZE, int COL_SIZE) {
    char c;
    for (int i = 0; i < ROW_SIZE; i++) {
        printf("[");
        for (int j = 0; j < COL_SIZE; j++) {
            c = matrix[j + (i * COL_SIZE)];
            if (j==COL_SIZE-1) { //Non mi trovo vicino al bordo e quindi NON aggiungo uno spazio a destra
                if (c == ALIVE) {
                    printf("%s", ALIVE_EMOJI);
                }
                else {
                    printf("%s", DEAD_EMOJI);
                }
            }
            else {//Mi trovo vicino al bordo e quindi AGGIUNGO uno spazio a destra
                if (c == ALIVE) {
                        printf("%s ", ALIVE_EMOJI);
                    }
                    else {
                        printf("%s ", DEAD_EMOJI);
                    }
            }
        }
        printf("]\n");
    }
}

void printMatrix(char *matrix, int ROW_SIZE, int COL_SIZE) {
    char c;
    for (int i = 0; i < ROW_SIZE; i++) {
        printf("[");
        for (int j = 0; j < COL_SIZE; j++) {
            c = matrix[j + (i * COL_SIZE)];
            if (j==COL_SIZE-1) {
                printf("%c", c);
            }
            else {
                printf("%c ", c);
            }
        }
        printf("]\n");
    }
}

//Distribuisce in maniera equa (se possibile) le righe di una matrice ai diversi processori
void initDisplacementPerProcess(int send_counts[], int displacements[], int rowPerProcess[], int nprocs, int resto, int divisione, int COL_SIZE) {
    for (int i = 0; i < nprocs; i++) {
        rowPerProcess[i] = (i < resto) ? divisione + 1 : divisione;                 //calcolo il numero di righe per processo, se riesce equamente altrimenti una riga in più ai processi con rank<resto
        send_counts[i] = (rowPerProcess[i]) * COL_SIZE;                             //calcolo il numero di elementi per ogni processo in base al numero di righe
        displacements[i] = i == 0 ? 0 : displacements[i - 1] + send_counts[i - 1];  //calcolo il displacements tra gli elementi dei processi
    }
}

//Funzioni di Inizializzazione matrice
void generateMatrix(char *matrix, int ROW_SIZE, int COL_SIZE) {
    int k=0;
    for (int i = 0; i < ROW_SIZE; i++) {
        for (int j = 0; j < COL_SIZE; j++, k++) {
            if (rand() % 2 == 0) {
                matrix[j + (i * COL_SIZE)] = DEAD;
            } else
                matrix[j + (i * COL_SIZE)] = ALIVE;
        }
    }
}

//Funzione che permette di ricostruire la sottomatrice del processo una volta ricevute le due righe 
//dai processi adiacenti (bottomRow e topRow)
void rebuildMatrix(char *matrixrec, int rows, char bottomRow[], char topRow[], char *rebuildedMatrix, int COL_SIZE) {
    int step = 0;

    for (int i = 0; i < 1; i++) { //Inserimento della prima riga
        for (int j = step, z = 0; j < step + COL_SIZE; j++, z++) {
            rebuildedMatrix[j] = bottomRow[z];
        }
        step = step + COL_SIZE;
    }

    int step1 = 0;
    for (int i = 0; i < rows; i++) { //Inserimento della matrice in mezzo
        for (int j = step1, z = step; j < step1 + COL_SIZE && z < step + COL_SIZE; j++, z++) {
            rebuildedMatrix[z] = matrixrec[j];
        }
        step = step + COL_SIZE;
        step1 = step1 + COL_SIZE;
    }
    for (int i = rows + 1; i < rows + 2; i++) { //Inserimento dell'ultima riga
        for (int j = step, z = 0; j < step + COL_SIZE; j++, z++) {
            rebuildedMatrix[j] = topRow[z];
        }
    }
}

void rebuildMatrix2(char *matrixrec, int edge_or_center, int rows, char bottomRow[], char topRow[], char *rebuildedMatrix, int COL_SIZE) {
    
    if (edge_or_center == 0) { //Inserimento righe estreme
        int step = 0;

        for (int i = 0; i < 1; i++) { //Inserimento  prima riga
            for (int j = step, z = 0; j < step + COL_SIZE; j++, z++) {
                rebuildedMatrix[j] = bottomRow[z];
            }
            step = step + COL_SIZE;
        }

        step = COL_SIZE + (rows * COL_SIZE);
        for (int i = rows + 1; i < rows + 2; i++) { //Inserimento ultima riga
            for (int j = step, z = 0; j < step + COL_SIZE; j++, z++) {
                rebuildedMatrix[j] = topRow[z];
            }
        }
    }
    else if (edge_or_center == 1) { //Inserimento matrice centrale
        int step = COL_SIZE;
        int step1 = 0;
        for (int i = 0; i < rows; i++) {
            for (int j = step1, z = step; j < step1 + COL_SIZE && z < step + COL_SIZE; j++, z++) {
                rebuildedMatrix[z] = matrixrec[j];
            }
            step = step + COL_SIZE;
            step1 = step1 + COL_SIZE;
        }
    }
}

//Funzione che  permette di fare il check di over and under population
char checkUnderAndOverPopulation(char *matrixrec, int currentRow, int currentColumn, int numbersOfRows, int numbersOfCols, int COL_SIZE) {
    int viciniVivi = 0;

    //Se la riga ha un indice maggiore di 0 ha sicuramente un vicino sopra di lei
    if (currentRow > 0) {
        viciniVivi = (matrixrec[((currentRow - 1) * COL_SIZE) + (currentColumn)] == ALIVE) ? viciniVivi + 1 : viciniVivi;
        //Ho il vicino sinistro
        if (currentColumn > 0)
            viciniVivi = (matrixrec[((currentRow - 1) * COL_SIZE) + (currentColumn - 1)] == ALIVE) ? viciniVivi + 1 : viciniVivi;
        //Ho il vicino destro
        if (currentColumn < numbersOfCols - 1)
            viciniVivi = (matrixrec[((currentRow - 1) * COL_SIZE) + (currentColumn + 1)] == ALIVE) ? viciniVivi + 1 : viciniVivi;
    }
    //Se la riga è minore del numero di righe-1, vuol dire che non è l'ultima ed ha un vicino sotto di lei
    if (currentRow < numbersOfRows - 1) {
        viciniVivi = (matrixrec[((currentRow + 1) * COL_SIZE) + (currentColumn)] == ALIVE) ? viciniVivi + 1 : viciniVivi;
        //Se la colonna ha un indice maggiore di 0 sicuramente ha un vicino sinistro
        if (currentColumn > 0)
            viciniVivi = (matrixrec[((currentRow + 1) * COL_SIZE) + (currentColumn - 1)] == ALIVE) ? viciniVivi + 1 : viciniVivi;
        //Se la colonna non è l'ultima ha sicuramente un vicino destro
        if (currentColumn < numbersOfCols - 1)
            viciniVivi = (matrixrec[((currentRow + 1) * COL_SIZE) + (currentColumn + 1)] == ALIVE) ? viciniVivi + 1 : viciniVivi;
    }
    //Se non sono presenti vicini top o bottom, controlliamo right or left

    //Controllo vicino sinistro
    if (currentColumn > 0)
        viciniVivi = (matrixrec[((currentRow)*COL_SIZE) + (currentColumn - 1)] == ALIVE) ? viciniVivi + 1 : viciniVivi;
    //Controllo vicino destro
    if (currentColumn < numbersOfCols - 1)
        viciniVivi = (matrixrec[((currentRow)*COL_SIZE) + (currentColumn + 1)] == ALIVE) ? viciniVivi + 1 : viciniVivi;

    //OverPopulation or UnderPopulation
    return (viciniVivi > 3 || viciniVivi < 2) ? DEAD : ALIVE;
}

//Funzione che  permette di controllare se una cella può riprodursi
char checkReproduction(char *matrixrec, int currentRow, int currentColumn, int numbersOfRows, int numbersOfCols, int COL_SIZE) {
    int viciniVivi = 0;

    //Se la riga ha un indice maggiore di 0 ha sicuramente un vicino sopra di lei
    if (currentRow > 0) {
        viciniVivi = (matrixrec[((currentRow - 1) * COL_SIZE) + (currentColumn)]) == ALIVE ? viciniVivi + 1 : viciniVivi;
        //Se la colonna ha un indice maggiore di 0 sicuramente ha un vicino sinistro
        if (currentColumn > 0)
            viciniVivi = (matrixrec[((currentRow - 1) * COL_SIZE) + (currentColumn - 1)]) == ALIVE ? viciniVivi + 1 : viciniVivi;
        //Se la colonna non è l'ultima ha sicuramente un vicino destro, Right neigh
        if (currentColumn < numbersOfCols - 1)
            viciniVivi = (matrixrec[((currentRow - 1) * COL_SIZE) + (currentColumn + 1)]) == ALIVE ? viciniVivi + 1 : viciniVivi;
    }
    //Se la riga è minore del numero di righe-1, vuol dire che non è l'ultima ed ha un vicino sotto di lei
    if (currentRow < numbersOfRows - 1) {
        viciniVivi = (matrixrec[((currentRow + 1) * COL_SIZE) + (currentColumn)]) == ALIVE ? viciniVivi + 1 : viciniVivi;
        //Se la colonna ha un indice maggiore di 0 sicuramente ha un vicino sinistro
        if (currentColumn > 0)
            viciniVivi = (matrixrec[((currentRow + 1) * COL_SIZE) + (currentColumn - 1)]) == ALIVE ? viciniVivi + 1 : viciniVivi;
        //Se la colonna non è l'ultima ha sicuramente un vicino destro
        if (currentColumn < numbersOfCols - 1)
            viciniVivi = (matrixrec[((currentRow + 1) * COL_SIZE) + (currentColumn + 1)]) == ALIVE ? viciniVivi + 1 : viciniVivi;
    }
    //Se non sono presenti vicini top o bottom, controlliamo right or left

    //Controllo vicino sinistro
    if (currentColumn > 0)
        viciniVivi = (matrixrec[((currentRow)*COL_SIZE) + (currentColumn - 1)]) == ALIVE ? viciniVivi + 1 : viciniVivi;
    //Controllo vicino destro
    if (currentColumn < numbersOfCols - 1)
        viciniVivi = (matrixrec[((currentRow)*COL_SIZE) + (currentColumn + 1)]) == ALIVE ? viciniVivi + 1 : viciniVivi;

    //Se il numero di viciniVivi è esattamente 3 la cella si riproduce altrimenti resta morta
    return (viciniVivi == 3) ? ALIVE : DEAD;
}

//Funzione core che aggiorna la porzione di matrice di ogni processo
void gameUpdate(char *rebuildedMatrix, char *newMiniWorld, int numbersOfRows, int rank, int COL_SIZE) {
    int offset_row_start = 1;                //Indica da dove deve partire la computazione per evitare di computare anche le ghost cells
    int offset_row_end = numbersOfRows + 1;  //Indica il punto dove deve finire la computazione evitando le ghost cells

    for (int i = offset_row_start; i < offset_row_end; i++) {
        for (int j = 0; j < COL_SIZE; j++) {
            //Se la cella è viva, allora controllo se deve morire o sopravvivere alla generazione successiva
            if (rebuildedMatrix[j + (i * COL_SIZE)] == ALIVE) {
                //sottraendo offset_row_start ad i ottengo la posizione corretta di dove inserire nel newMiniWorld
                newMiniWorld[j + ((i - offset_row_start) * COL_SIZE)] = checkUnderAndOverPopulation(rebuildedMatrix, i, j, offset_row_end + 1, COL_SIZE, COL_SIZE);
                //Se la cella è morta allora controllo se deve riprodursi
            } else if (rebuildedMatrix[j + (i * COL_SIZE)] == DEAD) {
                //sottraendo offset_row_start ad i ottengo la posizione corretta di dove inserire nel newMiniWorld
                newMiniWorld[j + ((i - offset_row_start) * COL_SIZE)] = checkReproduction(rebuildedMatrix, i, j, offset_row_end + 1, COL_SIZE, COL_SIZE);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int my_rank;        
    int nprocs;         
    int tag = 10;       
    double start_time, end_time;
    MPI_Status status;
    MPI_Request requestTop, requestBottom;
    
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);    
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);     
    srand(1);

    char *matrix; //Matrice generata

    /*Ottengo i parametri in input, necessari per creare la matrice ed il numero di generazioni*/
    int ROW_SIZE = atoi(argv[1]);       //Numero righe matrice
    int COL_SIZE = atoi(argv[2]);       //Numero colonne matrice
    int GENERATIONS = atoi(argv[3]);    //Numero generazioni

    // In ricezione dalla MPI_Scatterv
    char *matrixrec;
    int dimMatrixRec = 0;

    //Dati utili per la suddivisione in maniera equa (se possibile) della matrice
    int *counts;
    int *displacements; 
    int *rowPerProcess;

    // Dati per la ricostruzione della matrice
    char *rebuildedMatrix;
    char *topRow, *bottomRow;

    // Dati finali
    char *newMiniWorld;        //Porzione di matrice aggiornata
    char *finalWorld;          //Matrice che conterrà tutte le porzioni di matrici aggiornate
    
    int root_rank = 0;
        
    if (nprocs == 1) {
        matrix = (char *)malloc((ROW_SIZE * COL_SIZE) * sizeof(char));      
        topRow = malloc(sizeof(char) * COL_SIZE);                           
        bottomRow = malloc(sizeof(char) * COL_SIZE);                        
        rebuildedMatrix = malloc(sizeof(char) * (ROW_SIZE + 2) * COL_SIZE); //Alloco spazio per la matrice che verrà ricostruita, aggiungendo semplicemente "+ 2" per le due righe nuove
        finalWorld = malloc(sizeof(char) * (ROW_SIZE * COL_SIZE));

        generateMatrix(matrix, ROW_SIZE, COL_SIZE);

        start_time = MPI_Wtime();

        for (int curr_generation=0; curr_generation<GENERATIONS; curr_generation++) {
            memcpy(topRow, &matrix[0], COL_SIZE * sizeof(char));
            memcpy(bottomRow, &matrix[(ROW_SIZE - 1) * COL_SIZE], COL_SIZE * sizeof(char));

            //Ricostruisco la matrice
            rebuildMatrix(matrix, ROW_SIZE, bottomRow, topRow, rebuildedMatrix, COL_SIZE);
            
            //Aggiorno la matrice
            gameUpdate(rebuildedMatrix, finalWorld, ROW_SIZE, my_rank, COL_SIZE);

            //Aggiorno matrix affinché venga utilizzata la matrice aggiornata nella generazione successiva
            matrix = finalWorld;
        }
    }
    else {
        //Controllo se il numero di processi è maggiore del numero delle righe 
        MPI_Group world_group;  
        MPI_Comm_group(MPI_COMM_WORLD, &world_group);
        MPI_Group new_group;    
        MPI_Comm new_comm;      
        if (nprocs > ROW_SIZE) {
            int ranks_needed = nprocs > ROW_SIZE ? ROW_SIZE : nprocs;
            int ranks[ranks_needed];
            for (int i = 0; i < ranks_needed; i++) {
                ranks[i] = i;
            }
            MPI_Group_incl(world_group, ranks_needed, ranks, &new_group);
            MPI_Comm_create(MPI_COMM_WORLD, new_group, &new_comm);
        } else {
            MPI_Comm_create(MPI_COMM_WORLD, world_group, &new_comm);
        }
        
        //Elimino i processi non necessari
        if (new_comm == MPI_COMM_NULL) {
            //printf("Process %d, muoio. ", my_rank);
            MPI_Finalize();
            return 0;
        }

        MPI_Comm_rank(new_comm, &my_rank);
        MPI_Comm_size(new_comm, &nprocs);

        //Controllo chi è il processo precedente e successivo
        int next = (my_rank + 1) % nprocs;           
        int prev = (my_rank + nprocs - 1) % nprocs;  
        
        //Dati utili per la suddivisione in maniera equa (se possibile) della matrice
        int count = ROW_SIZE / nprocs;
        int remainder = ROW_SIZE % nprocs;
        int rows;

        counts = malloc(nprocs * sizeof(int));         //Array che conterrà il num. di elementi di ogni processo
        displacements = malloc(nprocs * sizeof(int));  //Array che conterrà i displacements per ogni processo
        rowPerProcess = malloc(nprocs * sizeof(int));  //Array che conterrà il numero di righe per ogni processo

        /* ------------ START ------------*/
        //Chiamata funzione di inizializzazione per displacement e send_counts per i processi
        initDisplacementPerProcess(counts, displacements, rowPerProcess, nprocs, remainder, count, COL_SIZE);

        matrixrec = malloc(sizeof(char) * (rowPerProcess[my_rank] * COL_SIZE));     //Alloco spazio per la matrice in ricezione
        dimMatrixRec = rowPerProcess[my_rank] * COL_SIZE;                           //Calcolo dimensione della matrice in ricezione

        //Inizializzazione delle variabili necessarie per l'invio delle righe ai processi adiacenti
        newMiniWorld = malloc(sizeof(char) * (rowPerProcess[my_rank]) * COL_SIZE);   
        rows = rowPerProcess[my_rank];                //N. di righe della porzione di array ricevuta
        topRow = malloc(sizeof(char) * COL_SIZE);           
        bottomRow = malloc(sizeof(char) * COL_SIZE);        
        
        rebuildedMatrix = malloc(sizeof(char) * (rowPerProcess[my_rank] + 2) * COL_SIZE);  //Alloco spazio per la matrice che verrà ricostruita, aggiungendo semplicemente "+ 2" per le due righe nuove (ottenute dai processi adiacenti)
        
        if(my_rank == root_rank) { //* MASTER
            //Generazione iniziale e stampa della matrice
            matrix = (char *)malloc((ROW_SIZE * COL_SIZE) * sizeof(char));
            generateMatrix(matrix, ROW_SIZE, COL_SIZE);
            finalWorld = malloc(sizeof(char) * (ROW_SIZE * COL_SIZE));
        }
        else { //TODO: SLAVE
            /*VUOTO*/
        }
        
        start_time = MPI_Wtime();
        
        //Invio e ricezione della porzione di matrice ad ogni processo
        MPI_Scatterv(matrix, counts, displacements, MPI_CHAR, matrixrec, dimMatrixRec, MPI_CHAR, root_rank, new_comm);
        
        for (int curr_generation=0; curr_generation<GENERATIONS; curr_generation++) {
            MPI_Isend(&matrixrec[0], COL_SIZE, MPI_CHAR, prev, tag, new_comm, &requestTop);    //Invio la riga al mio prev

            MPI_Isend(&matrixrec[(rows - 1) * COL_SIZE], COL_SIZE, MPI_CHAR, next, tag, new_comm, &requestBottom); //Invio la riga al mio next
        
            //Nel frattempo inizio a ricostruire la matrice inserendo quella centrale
            rebuildMatrix2(matrixrec, 1, rows, NULL, NULL, rebuildedMatrix, COL_SIZE);
            
            //Ricevo le righe dai processi adiacenti
            MPI_Recv(bottomRow, COL_SIZE, MPI_CHAR, prev, tag, new_comm, &status); //Ricevo la riga dal mio prev

            MPI_Recv(topRow, COL_SIZE, MPI_CHAR, next, tag, new_comm, &status);    //Ricevo la riga dal mio next

            //Ricostruisco la sottomatrice del processo aggiungendo le 2 righe ottenute dai processi adiacenti
            if (prev == next) {
                //Se ho due processi, next e prev coincidono quindi devo scambiare "topRow" con "bottomRow"
                rebuildMatrix2(NULL, 0, rows, topRow, bottomRow, rebuildedMatrix, COL_SIZE);
            }
            else {
                //Se ho più di due processi NON devo invertire "topRow" con "bottomRow"
                rebuildMatrix2(NULL, 0, rows, bottomRow, topRow, rebuildedMatrix, COL_SIZE);
            }

            //Chiamo la funzione principale "gameUpdate" per aggiornare le matrici (e quindi sto aggiornando il gioco)
            gameUpdate(rebuildedMatrix, newMiniWorld, rows, my_rank, COL_SIZE);

            //Adesso aggiorno matrixrec affinché venga utilizzata la porzione aggiornata della matrice nella generazione successiva
            matrixrec = newMiniWorld;
        }
        
        //Restituisco la matrice finale contenuta in finalWorld al master per la stampa
        MPI_Gatherv(newMiniWorld, counts[my_rank], MPI_CHAR, finalWorld, counts, displacements, MPI_CHAR, root_rank, new_comm);
    }
    

    end_time = MPI_Wtime();
    /*-------------FINE-------------*/
    //libero spazio
    if (nprocs == 1) {
        free(rebuildedMatrix);
        free(topRow);
        free(bottomRow);
        free(matrix);
    }
    else  {
        free(counts);
        free(displacements);
        free(rowPerProcess);

        free(matrixrec);

        free(rebuildedMatrix);
        free(topRow);
        free(bottomRow);
        
        if (my_rank == root_rank) {
            free(finalWorld);
            free(matrix);
        }
    }
    
    if (my_rank == root_rank) {
        printf("\nNprocs: %d, Time in seconds=%f\n", nprocs, end_time - start_time);
    }
    MPI_Finalize();
    return 0;
}
