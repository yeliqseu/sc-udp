#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Jul 19 14:37:56 2021

@author: yeli
"""

import sys
import math
import numpy as np
from scipy import constants
import matplotlib.pyplot as plt
import subprocess
import pandas as pd

rtt = "highRTT"
pe = 0.01
isfec = "false"

basestring1 = "../simdata/"
basestring2 = rtt+"-tcp-"
basestring3 = "-1-pe"+str(pe)+"-buf1000ADU-FEC-"+isfec+".log"

# Call awk script to process log
subprocess.run(["./process_scudp_data.sh", 
                "../simdata/"+rtt+"-scudp-1-pe"+str(pe)+"-trueInitBw-pacing-2.log"])
subprocess.run(["mv", "/tmp/Goodput.dat", "/tmp/Goodput-scudp.dat"])



subprocess.run(["./process_scudp_data.sh", 
                basestring1 + basestring2 + "Bbr" + basestring3]);
subprocess.run(["mv", "/tmp/Goodput.dat", "/tmp/Goodput-bbr.dat"])
subprocess.run(["mv", "/tmp/Tcp-Time.dat", "/tmp/Tcp-Time-bbr.dat"])
subprocess.run(["./process_tcp_single_cwnd.sh", 
                basestring1 + basestring2 + "Bbr" + basestring3]);
subprocess.run(["mv", "/tmp/cwnd.dat", "/tmp/Tcp-cwnd-Bbr.dat"])

subprocess.run(["./process_scudp_data.sh", 
                basestring1 + basestring2 + "Copa" + basestring3]);
subprocess.run(["mv", "/tmp/Goodput.dat", "/tmp/Goodput-copa.dat"])
subprocess.run(["mv", "/tmp/Tcp-Time.dat", "/tmp/Tcp-Time-copa.dat"])
subprocess.run(["./process_tcp_single_cwnd.sh", 
                basestring1 + basestring2 + "Copa" + basestring3]);
subprocess.run(["mv", "/tmp/cwnd.dat", "/tmp/Tcp-cwnd-Copa.dat"])


subprocess.run(["./process_scudp_data.sh", 
                basestring1 + basestring2 + "Cubic" + basestring3]);
subprocess.run(["mv", "/tmp/Goodput.dat", "/tmp/Goodput-cubic.dat"])
subprocess.run(["mv", "/tmp/Tcp-Time.dat", "/tmp/Tcp-Time-cubic.dat"])
subprocess.run(["./process_tcp_single_cwnd.sh", 
                basestring1 + basestring2 + "Cubic" + basestring3]);
subprocess.run(["mv", "/tmp/cwnd.dat", "/tmp/Tcp-cwnd-Cubic.dat"])


# BBR-dFEC
isfec = "true"
basestring3 = "-1-pe"+str(pe)+"-buf1000ADU-FEC-"+isfec+".log"
subprocess.run(["./process_scudp_data.sh", 
                basestring1 + basestring2 + "Bbr" + basestring3]);
subprocess.run(["mv", "/tmp/Goodput.dat", "/tmp/Goodput-bbr-dFEC.dat"])
subprocess.run(["mv", "/tmp/Tcp-Time.dat", "/tmp/Tcp-Time-bbr-dFEC.dat"])
subprocess.run(["./process_tcp_single_cwnd.sh", 
                basestring1 + basestring2 + "Bbr" + basestring3]);
subprocess.run(["mv", "/tmp/cwnd.dat", "/tmp/Tcp-cwnd-Bbr-dFEC.dat"])



gpt_scudp = np.loadtxt("/tmp/Goodput-scudp.dat")
gpt_copa  = np.loadtxt("/tmp/Goodput-copa.dat")
gpt_bbr   = np.loadtxt("/tmp/Goodput-bbr.dat")
gpt_cubic = np.loadtxt("/tmp/Goodput-cubic.dat")
gpt_bbr_dFEC   = np.loadtxt("/tmp/Goodput-bbr-dFEC.dat")

time_copa  = np.loadtxt("/tmp/Tcp-Time-copa.dat")
time_bbr  = np.loadtxt("/tmp/Tcp-Time-bbr.dat")
time_cubic  = np.loadtxt("/tmp/Tcp-Time-cubic.dat")
time_bbr_dFEC  = np.loadtxt("/tmp/Tcp-Time-bbr-dFEC.dat")

# Call awk script to process log
subprocess.run(["./process_scudp_data.sh", 
                "../simdata/"+rtt+"-scudp-1-pe"+str(pe)+"-trueInitBw-pacing-2.log"])


estfname = "/tmp/Estimation.dat"
fecfname = "/tmp/Fec.dat"
winfname = "/tmp/Win.dat"
gptfname = "/tmp/Goodput.dat"
timfname = "/tmp/Time.dat"
losfname = "/tmp/Loss.dat"
appPktSize = 1440 # in bytes


est = np.loadtxt(estfname)
fec = np.loadtxt(fecfname)
win = np.loadtxt(winfname)
gpt = np.loadtxt(gptfname)
los = np.loadtxt(losfname)
tim = pd.read_csv(timfname, delim_whitespace=True, names=['hostID', 'pktType', 'pktID', 'sendTime', 'recvTime', 'ackTime', 'deliverTime'])

hostid = 1;

# CWND change data for pe=0.1%
cwnd_scudp = np.loadtxt("../simdata/processed/"+rtt+"-scudp-pe"+str(pe)+"-cwnd.dat")
cwnd_copa = np.loadtxt("/tmp/Tcp-cwnd-Copa.dat")
cwnd_bbr = np.loadtxt("/tmp/Tcp-cwnd-Bbr.dat")
cwnd_cubic = np.loadtxt("/tmp/Tcp-cwnd-Cubic.dat")
cwnd_bbr_dFEC = np.loadtxt("/tmp/Tcp-cwnd-Bbr-dFEC.dat")


# fig, axs = plt.subplots(3, 1, figsize=(12,12))
# axs[0].plot(gpt_scudp[gpt_scudp[:,0]==hostid,1], gpt_scudp[gpt_scudp[:,0]==hostid,2]*8/1e6,
#         gpt_copa[gpt_copa[:,0]==hostid,1], gpt_copa[gpt_copa[:,0]==hostid,2]*8/1e6,
#         gpt_bbr[gpt_bbr[:,0]==hostid,1], gpt_bbr[gpt_bbr[:,0]==hostid,2]*8/1e6,
#         gpt_cubic[gpt_cubic[:,0]==hostid,1], gpt_cubic[gpt_cubic[:,0]==hostid,2]*8/1e6
#         ) 
# axs[0].set_xlim([0, 35])
# axs[0].legend(['SC-UDP', 'Copa', 'BBR', 'Cubic'], frameon=True, fontsize=14)
# axs[0].set_xlabel('Time (s)',fontsize=14)
# axs[0].set_ylabel('Cumulative Goodput (Mbps)',fontsize=14)
# axs[0].grid(True, color='grey', linestyle='--')

# axs[1].plot(cwnd_scudp[:,0], cwnd_scudp[:,1],
#            cwnd_copa[:,0], cwnd_copa[:,1],
#            cwnd_bbr[:,0], cwnd_bbr[:,1],
#            cwnd_cubic[:,0], cwnd_cubic[:,1])
# axs[1].set_xlim([0, 35])
# axs[1].legend(['SC-UDP', 'Copa', 'BBR', 'Cubic'], frameon=True, fontsize=14)
# axs[1].set_xlabel('Time (s)',fontsize=14)
# axs[1].set_ylabel('CWND (# pkts.)',fontsize=14)
# axs[1].grid(True, color='grey', linestyle='--')

# # compare delivery time
# tim1 = tim[ (tim["hostID"]==hostid) & tim["pktType"].str.match('S') ]
# ddelay = (tim1['deliverTime'] - tim1['sendTime'])
# axs[2].plot(tim1['pktID'], ddelay,
#             time_copa[:,0], time_copa[:,3],
#             time_bbr[:,0], time_bbr[:,3],
#             time_cubic[:,0], time_cubic[:,3])
# axs[2].set_xlabel('Source Packet ID',fontsize=14)
# axs[2].set_ylabel('Delivery Delay (s)',fontsize=14)
# axs[2].set_xlim([0, tim1['pktID'].max()])
# axs[2].legend(['SC-UDP', 'Copa', 'BBR', 'Cubic'], frameon=True,fontsize=14)
# axs[2].grid(True, color='grey', linestyle='--')

# plt.savefig('single_goodput.eps',bbox_inches='tight')


fig, axs = plt.subplots(3, 1, figsize=(12,12))
axs[0].plot(gpt_scudp[gpt_scudp[:,0]==hostid,1], gpt_scudp[gpt_scudp[:,0]==hostid,2]*8/1e6,
        gpt_copa[gpt_copa[:,0]==hostid,1], gpt_copa[gpt_copa[:,0]==hostid,2]*8/1e6,
        gpt_bbr[gpt_bbr[:,0]==hostid,1], gpt_bbr[gpt_bbr[:,0]==hostid,2]*8/1e6,
        gpt_bbr_dFEC[gpt_bbr_dFEC[:,0]==hostid,1], gpt_bbr_dFEC[gpt_bbr_dFEC[:,0]==hostid,2]*8/1e6
        ) 
axs[0].legend(['SC-UDP', 'Copa', 'BBR', 'BBR-dFEC'], frameon=True, fontsize=14)
axs[0].set_xlabel('Time (s)',fontsize=14)
axs[0].set_ylabel('Cumulative Goodput (Mbps)',fontsize=14)
axs[0].grid(True, color='grey', linestyle='--')

axs[1].plot(cwnd_scudp[:,0], cwnd_scudp[:,1],
           cwnd_copa[:,0], cwnd_copa[:,1],
           cwnd_bbr[:,0], cwnd_bbr[:,1],
           cwnd_bbr_dFEC[:,0], cwnd_bbr_dFEC[:,1])

axs[1].legend(['SC-UDP', 'Copa', 'BBR', 'BBR-dFEC'], frameon=True, fontsize=14)
axs[1].set_xlabel('Time (s)',fontsize=14)
axs[1].set_ylabel('CWND (# pkts.)',fontsize=14)
axs[1].grid(True, color='grey', linestyle='--')

# compare delivery time
tim1 = tim[ (tim["hostID"]==hostid) & tim["pktType"].str.match('S') ]
ddelay = (tim1['deliverTime'] - tim1['sendTime'])
axs[2].plot(tim1['pktID'], ddelay,
            time_copa[:,0], time_copa[:,3],
            time_bbr[:,0], time_bbr[:,3],
            time_bbr_dFEC[:,0], time_bbr_dFEC[:,3])
axs[2].set_xlabel('Source Packet ID',fontsize=14)
axs[2].set_ylabel('Delivery Delay (s)',fontsize=14)
axs[2].set_xlim([0, tim1['pktID'].max()])
axs[2].legend(['SC-UDP', 'Copa', 'BBR', 'BBR-dFEC'], frameon=True,fontsize=14)
axs[2].grid(True, color='grey', linestyle='--')

#plt.savefig('single_goodput_2.eps',bbox_inches='tight')





# fig, axs = plt.subplots(3,2,figsize=(12, 12))
# est1 = est[est[:,0]==hostid, :]
# axs[0][0].plot(est1[:,1], est1[:,2]* appPktSize * 8 / 1e6)
# axs[0][0].set_xlabel('Time (s)',fontsize=14)
# axs[0][0].set_ylabel('Estimated Bandwidth (Mbps)',fontsize=14)
# axs[0][0].set_xticks(np.arange(0, 25, step=5))
# axs[0][0].grid(True)

# est1 = est[est[:,0]==hostid, :]
# axs[0][1].plot(est1[:,1], est1[:,6])
# axs[0][1].set_xlabel('Time (s)',fontsize=14)
# axs[0][1].set_ylabel('Instantaneous RTT (ms)',fontsize=14)
# axs[0][1].set_xticks(np.arange(0, 25, step=5))
# axs[0][1].grid(True)

# axs[1][0].plot(est1[:,1], est1[:,4])
# axs[1][0].set_xlabel('Time (s)',fontsize=14)
# axs[1][0].set_ylabel('Loss Rate',fontsize=14)
# axs[1][0].set_xticks(np.arange(0, 25, step=5))
# axs[1][0].grid(True)

# fec1 = fec[fec[:,0]==hostid, :]
# axs[1][1].plot(fec1[:,1], fec1[:,2])
# axs[1][1].set_xlabel('Time (s)',fontsize=14)
# axs[1][1].set_ylabel('FEC Code Rate',fontsize=14)
# axs[1][1].grid(True)


# win1 = win[win[:,0]==hostid, :]
# # Fetch average decoding window size of the host from Time.dat
# avgwin = float(tim[ (tim["hostID"]==hostid) & tim["pktID"].isnull() ]["pktType"].values[0])
# axs[2][0].plot(win1[:,1], win1[:,2])
# axs[2][0].axhline(y=avgwin, color='r', linestyle='--')
# axs[2][0].set_xlabel('Time (s)',fontsize=14)
# axs[2][0].set_ylabel('Decoding Window (#pkts)',fontsize=14)
# axs[2][0].legend(['Window Length', 'Average Window Length'],fontsize=14)
# axs[2][0].grid(True)

# tim1 = tim[ (tim["hostID"]==hostid) & tim["pktType"].str.match('S') ]
# ddelay = (tim1['deliverTime'] - tim1['sendTime'])
# axs[2][1].plot(tim1['pktID'], ddelay)
# axs[2][1].axhline(y=ddelay.mean(), color='r', linestyle='--')
# axs[2][1].set_xlabel('Source Packet ID',fontsize=14)
# axs[2][1].set_ylabel('Time (s)',fontsize=14)
# axs[2][1].set_xlim([0, tim1['pktID'].max()])
# axs[2][1].legend(['Packet Delivery Time', 'Average Delivery Time'],fontsize=14)
# axs[2][1].grid(True)



# plt.savefig('single_scudp_metrics.eps',bbox_inches='tight')

