set terminal pdf
set output "treated_result.pdf"

set xlabel "trace depth"

set ylabel "branch resolved rate (%)"
set yrange [0:100]

set key title "rollback number"

plot "treated_result.10000" with linespoints title "10000", \
	 "treated_result.20000" with linespoints title "20000", \
	 "treated_result.30000" with linespoints title "30000"

#plot "treated_result.20000" with linespoints title "20000"