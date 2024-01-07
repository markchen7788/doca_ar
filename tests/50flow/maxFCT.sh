for i in {1..30}
do
	#iperf -c 192.168.200.1 -P 10 -n 1g |grep SUM >> 1.txt
	echo 'do test in ' $((i)) ' times'
	iperf -c 192.168.233.1 -P 50 -n 5m |grep SUM >> 1.txt
	sleep 3	
done
