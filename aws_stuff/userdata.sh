git clone https://github.com/gmatheus95/mo802_project.git &&
cd mo802_project &&
chmod +x *.sh &&
chmod +x runtime/*.sh &&
./run-spits-serial.sh 960 1079 &&
./run-spits-serial.sh 960 540 &&
./run-spits-serial.sh 480 270 &&
./run-spits-serial.sh 240 216 &&
./run-spits-serial.sh 192 108 &&
./run-spits-parallel.sh &&
path_to_s3="s3://guilherme-mo802/" &&
instance_type=$(wget -q -O - http://169.254.169.254/latest/meta-data/instance-type) &&
path_to_s3=$path_to_s3$(date "+%Y-%m-%d-%H-%M-%S")"/"$instance_type &&
aws s3 sync results/ $path_to_s3 