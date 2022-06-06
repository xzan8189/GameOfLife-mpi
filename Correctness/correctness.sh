bold=$(tput bold)
normal=$(tput sgr0)
np=1
max_np=10
ROW_SIZE=5
COL_SIZE=5
GENERATIONS=2

echo -e "\nCompilazione 'GameOfLife_corr.c'.."
mpicc GameOfLife_corr.c -o fin_corr

while [ $np -lt $max_np ]
do
    echo -e "\nEsecuzione e confronto con:\n ${bold}num_processi=$np${normal}, matrice=$ROW_SIZE x $COL_SIZE e num_generazioni=$GENERATIONS"
    mpirun --allow-run-as-root -np $np fin_corr $ROW_SIZE $COL_SIZE $GENERATIONS |& grep -v "Read -1"
    diff -c oracolo.txt risultato.txt

    ((np++))

done

echo -e "\nFine!\n"