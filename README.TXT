This program was put together by ECE 18.6 out of the Seattle University 
Department of Science and Engineering. The members of this group are:
Jesse Batstone, Trung Doan, Jinyanzi Luo and Anna Keziah Pidong.
This program inputs a series of waypoints (gps coordinates) which it uses to
traverse a path. In doing so it scans for rfid tags. The ouput of this program
is a series of tags with the gps coordinate and time of when/where it was 
scanned.

To compile the program use the 'make' command

To debug use 'gdb roboclaw'

To run use './roboclaw'
	Commands when running roboclaw:
		w,a,s,d control direction
		p initiates the waypoints directive
		q quits the program

roboclaw also calls ./autoread/c/src/receiveAutonomusReading (rfid scanning)
and ./link (matches gps coordinates to rfid tags based on time)

data from this program is stored in the txt files:
	gpsData.txt
	rfidData.txt
	linked.txt
