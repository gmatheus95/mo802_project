#!/usr/bin/python

import boto3
import base64
from botocore.exceptions import ClientError
import logging
import sys
import json
from datetime import datetime
import time

def getLogger(name):

    now = datetime.now()
    #Logging configuration
    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    #Log formatter
    formatter = logging.Formatter("[%(asctime)s] %(levelname)-8s %(message)s")
    #Log File handler
    handler = logging.FileHandler("create_spot_"+now.strftime('%d%b%Y%H%M')+".log")
    handler.setLevel(logging.INFO)
    handler.setFormatter(formatter)
    logger.addHandler(handler)
    #Screen handler
    screenHandler = logging.StreamHandler(stream=sys.stdout)
    screenHandler.setLevel(logging.INFO)
    screenHandler.setFormatter(formatter)
    logger.addHandler(screenHandler)
    return logger

def main():

    # now = datetime.now()
    # today = now.strftime("%d%b%Y")

    user_data = """#!/bin/bash

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
aws s3 sync results/ $path_to_s3"""

    if len(sys.argv) <= 1:
        logger.error('Please insert the json path as the first parameter! Terminating...')
        return
    logger.info('Starting the instance deployment from the template "'+sys.argv[1]+'"')
    
    # Opening json with definitions from the first argument
    machine_definitions = json.load(open(sys.argv[1],'r'))
    machine_definitions['LaunchSpecification']['UserData'] =  str(base64.b64encode(user_data))

    ec2 = boto3.client('ec2', region_name='us-east-1')    
    
    response  = ec2.request_spot_instances(**machine_definitions)

    # get the instance id from the only (if only 1) created instance
    spot_request_id = response['SpotInstanceRequests'][0]['SpotInstanceRequestId']
    logger.info('Spot Request done! Spot Request Id: '+spot_request_id)

    logger.info('All done.')
        

if __name__ == '__main__':
    logger = getLogger(__name__)
    main()

# ====== Some Spot Template \/ ======
# BlockDurationMinutes=60, # spot duration
# SpotPrice=PRICE, # take care about the price here, it changes from instance to instance
# InstanceCount=1, # number of instances to be deployed
# LaunchSpecification={
#     'InstanceType': instance_type,
#     'ImageId': 'ami-50be8446', # the Image can be a public one or a custom one, depending on your application
#     'SecurityGroups': ['generic_security_group'], # a list of security groups of this instance
#     'UserData': str(base64.b64encode(data_user)), #here you inset the shellscript to be executed when the machine get active - it must be a string from a base64 conversion
#     'KeyName': 'BRTeamKey', # here is the private key to access the instance remotely
#     'Monitoring': {
#         'Enabled': True
#     },
#     "IamInstanceProfile": {
#         "Arn": "arn:aws:iam::882732830129:instance-profile/generic_iam_role"
#     },
#     'BlockDeviceMappings':[
#     {
#         'DeviceName': '/dev/sdh',
#         'VirtualName': 'ephemeral0',
#         'Ebs': {
#             'Encrypted': False,
#             'DeleteOnTermination': True,
#             'VolumeSize': 30,
#             'VolumeType': 'gp2'
#         },
#     },
#     ]
# }