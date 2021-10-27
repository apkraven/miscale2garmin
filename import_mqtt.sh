# *********
# Mi Body Composition Scale 2 Garmin Connect v2.1
# *********

# Data acquisition from MQTT broker
user=admin
passwd=password
host=host_name
path=/home/robert

#Create a data backup file
if [ ! -f $path/data/backup.csv ] ; then
	echo '* Create a data backup file, checking for new data'
	echo 'Weight;Impedance;Units;User;Unix_time;Readable_time;Bat_in_V;Bat_in_%' > $path/data/backup.csv
else
	echo '* Data backup file exists, checking for new data'
fi

# Create file with import data
read_MQTT=`mosquitto_sub -h $host -t 'data' -u $user -P $passwd -C 1 | awk -F "\"*;\"*" 'END{print $5}'`
if grep -q $read_MQTT $path/data/backup.csv ; then
	echo '* There is no new data to upload to Garmin Connect'
elif [ -f $path/data/$read_MQTT.tlog ] ; then
	echo '* Import file already exists, calculating data to upload'
else
	mosquitto_sub -h $host -t 'data' -u $user -P $passwd -C 1 > $path/data/$read_MQTT.tlog
	echo '* Importing and calculating data to upload'
fi

# Calculate data and export to Garmin Connect, logging, handling errors, backup file
if [ -f $path/data/*.tlog ] ; then
	python3 $path/export_garmin.py > $path/data/temp.log 2>&1
	move=`awk -F ": " '/Processed file:/{print $2}' $path/data/temp.log`
	if grep -q 'Error' $path/data/temp.log ; then
		echo '* Errors detected, upload to Garmin Connect has failed'
	elif grep -q 'panic' $path/data/temp.log ; then
		echo '* Errors detected, upload to Garmin Connect has failed'
	elif grep -q 'denied' $path/data/temp.log ; then
		echo '* Errors detected, upload to Garmin Connect has failed'
	else
		cat $path/data/$move >> $path/data/backup.csv
		rm $path/data/$move
		echo '* Data upload to Garmin Connect is complete'
	fi
fi
