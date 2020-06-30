for load in $(seq 20 20 101);
do
    python boxplot-$load.py &
done
python cpu_utilization.py &
python boxplot-100-2.py &
