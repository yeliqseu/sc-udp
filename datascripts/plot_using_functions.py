#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Fri Jul  9 23:56:03 2021

@author: yeli
"""
import sys
import math
import numpy as np
from scipy import constants
import matplotlib.pyplot as plt
import subprocess
import pandas as pd
import os.path
from matplotlib.ticker import MultipleLocator, FormatStrFormatter, FuncFormatter
# 刻度标签生成函数，该函数只针对于本实验下设置次刻度标签为流数目
def formatterFunc(value, tickNumber) :
    midthInterval = 3
    flowNum = [1, 2, 5, 10]
    if int(value/midthInterval)-1 < 0 or int(value/midthInterval)-1 >= len(flowNum):
        return
    return "\n\n\n\n%s" % flowNum[int(value/midthInterval) - 1]

def plot_gpt_stacks(ax, n, RTT, peStr):
    if RTT == 0.6:
        baseRawPath = '../simdata/highRTT-'
        baseDatPath = '../simdata/processed/highRTT-'
    if RTT == 0.06:
        baseRawPath = '../simdata/lowRTT-'
        baseDatPath = '../simdata/processed/lowRTT-'
    
    timeInterval = n * RTT
    nPacket = 25000
    packetSize = 1440
    flowType = ['scudp', 'tcp-Copa', 'tcp-Bbr', 'tcp-Cubic']
    flowNum = [1, 2, 5, 10]
    for ft in flowType:
        for fn in flowNum:
            if ft=='scudp':
                logfile = baseRawPath + ft + '-' + str(fn) + peStr + '-trueInitBw-pacing-2.log'
            if ft=='tcp-Cubic':
                logfile = baseRawPath + ft + '-' + str(fn) + peStr + '-buf1000ADU.log'
            if ft=='tcp-Copa':
                logfile = baseRawPath + ft + '-' + str(fn) + peStr + '-buf1000ADU.log'
            if ft=='tcp-Bbr':
                logfile = baseRawPath + ft + '-' + str(fn) + peStr + '-buf1000ADU.log'
                
            outfile = baseDatPath + 'Goodputs-' + ft + '-' + str(fn) + peStr + '-' + str(n) + 'RTT.dat'
            if not os.path.exists(outfile):
                subprocess.run(["process_mixed_flow_goodputs.sh",
                                str(timeInterval), str(nPacket), str(packetSize),
                                logfile, outfile])
                
    # First, plot the goodputs of flows up to the first flow completes. The goodputs
    # are plotted as stacked bars
    # goodPuts保存流数目fn的实验中，方案为ft的用户吞吐量
    goodPuts = {}
    fairness = {}
    for fn in flowNum:
        goodPuts[fn] = {}
        fairness[fn] = {}
    
           
    for fn in flowNum:
        for ft in flowType:
            gptfname = baseDatPath + 'Goodputs-' + ft + '-' + str(fn) + peStr + '-' + str(n) + 'RTT.dat'
            gpt = np.loadtxt(gptfname)
            if gpt.ndim == 1:
                nFlow = 1
                nInterval = gpt.size - 1
                finalGpt = np.sum(gpt[1:])/nInterval/timeInterval * 8 / 1e6
                
                goodPuts[fn][ft] = np.array([finalGpt])
                # Calculate the long-term fair index
                fairness[fn][ft] = np.square(np.sum(finalGpt)) / (fn * np.sum(np.square(finalGpt)))
            else:
                nFlow = gpt.shape[0]
                nInterval = gpt.shape[1] - 1
                finalGpt = np.sum(gpt[:,1:],axis=1)/nInterval/timeInterval * 8 / 1e6
        
                goodPuts[fn][ft] = finalGpt
                # Calculate the long-term fair index
                fairness[fn][ft] = np.square(np.sum(finalGpt)) / (fn * np.sum(np.square(finalGpt)))
            
    #fig, ax = plt.subplots(figsize=(15, 9))
    #ax.set_xlabel('Number of Pairs',fontsize=12)
    ax.set_ylabel('Goodput (Mbps)',fontsize=12)
    
    # 每组实验圆柱的中心位置
    midthInterval = 3
    midth = np.arange(midthInterval, midthInterval * (len(flowNum) + 1), midthInterval)
    # 圆柱宽度
    columnWidth = 0.3
    # 圆柱位置以及位置间隔，注意图中最后相邻圆柱间隙应该是 posInterval-columnWidth
    columnPos = {}
    posInterval = 0.6
    offset = -3.0/2 * posInterval
    for ft in flowType:
        columnPos[ft] = []
        for mid in midth:
            columnPos[ft].append(mid + offset)
        offset += posInterval
        
    # 柱状图是每个方案依次绘制，就像垒砖一样将每个方案下用户的吞吐量圆柱叠加上去，一个方案一直垒
    for ft in flowType:
        for fn in flowNum:
            columnHeight = 0
            for gpt in goodPuts[fn][ft]:
                pos = columnPos[ft][flowNum.index(fn)]
                ax.bar(pos, gpt, columnWidth, bottom=columnHeight)
                columnHeight += gpt
    
    
    ax.minorticks_on()
    ax.xaxis.set_minor_locator(MultipleLocator(midthInterval + 0.0001))           # 将x次刻度标签设置为3的倍数
    ax.xaxis.set_tick_params(which='minor',labelsize=12)
    
    # 设置次刻度标签为用户流的数目
    # ax.xaxis.set_minor_formatter(FuncFormatter(formatterFunc))
    # ax.yaxis.set_minor_locator(MultipleLocator(2.5))
    ax.set_ylim([0, 20])
    
    ax.tick_params(labelsize=12)
    
    # 画网格线
    ax.grid(True, which='minor', color='grey', linestyle='--', alpha=0.5)
    
    # 设置主刻度标签为协议名
    xticks = []
    for ft in flowType:
        xticks += columnPos[ft]
    xticks.sort()
    ax.set_xticks(xticks)
    Type = ['SC-UDP', 'Copa', 'BBR', 'Cubic']
    titles = len(flowNum) * Type
    ax.set_xticklabels(titles, rotation=45, fontsize=10)
    

def plot_CoV(ax, fn, n, RTT, peStr):
    if RTT == 0.6:
        baseRawPath = '../simdata/highRTT-'
        baseDatPath = '../simdata/processed/highRTT-'

    if RTT == 0.06:
        baseRawPath = '../simdata/lowRTT-'
        baseDatPath = '../simdata/processed/lowRTT-'

    timeInterval = n * RTT
    nPacket = 25000
    packetSize = 1440
    flowType = ['scudp', 'tcp-Copa', 'tcp-Bbr', 'tcp-Cubic']
    markers = ['+', '-', 'x', 'o']
    for ft in flowType:
        gptfname = baseDatPath + 'Goodputs-' + ft + '-' + str(fn) + peStr + '-' + str(n) + 'RTT.dat'
        # gptfname = '/Users/yeli/Downloads/RngRun123.dat'
        gpt = np.loadtxt(gptfname)
        # Calcuate the coefficient of variation (CoV) of each flow
        gptsMean = np.mean(gpt[:,1:]/timeInterval/packetSize, axis=1)
        gptsStd = np.std(gpt[:,1:]/timeInterval/packetSize, axis=1)
        gptsCoV = gptsStd / gptsMean
        ax.plot(np.arange(1,fn+1), gptsCoV, label=ft, marker='+')


    typeLabel = ['SC-UDP', 'TCP Copa', 'TCP BBR', 'TCP Cubic']
    ax.set_xlabel('Flow ID', fontsize=12)
    ax.set_xticks(np.arange(1, fn+1))
    ax.set_ylabel('CoV', fontsize=12)
    ax.legend(typeLabel, frameon=True)
    ax.grid(linestyle='--')    


def plot_SFI(ax, fn, n, RTT, peStr):
    if RTT == 0.6:
        baseRawPath = '../simdata/highRTT-'
        baseDatPath = '../simdata/processed/highRTT-'
        
    if RTT == 0.06:
        baseRawPath = '../simdata/lowRTT-'
        baseDatPath = '../simdata/processed/lowRTT-'
        
    timeInterval = n * RTT
    nPacket = 25000
    packetSize = 1440
    flowType = ['scudp', 'tcp-Copa', 'tcp-Bbr', 'tcp-Cubic']
    fn = 10
    markers = ['+', '-', 'x', 'o']
    for ft in flowType:
        gptfname = baseDatPath + 'Goodputs-' + ft + '-' + str(fn) + peStr + '-' + str(n) + 'RTT.dat'
        # gptfname = '/Users/yeli/Downloads/RngRun123.dat'
        gpt = np.loadtxt(gptfname)
        # Calculate short-term fair index of the flows
        # print(gptfname)
        shortTermFairness = np.square(np.sum(gpt[:,1:], axis=0)) / (fn * np.sum(np.square(gpt[:,1:]), axis=0))
        ax.plot(shortTermFairness, label=ft)

    typeLabel = ['SC-UDP', 'TCP Copa', 'TCP BBR', 'TCP Cubic']
    ax.set_xlabel('Index of Observation Windows', fontsize=12)
    ax.set_ylabel('Short-Term Fairness', fontsize=12)
    ax.legend(typeLabel, frameon=True)
    ax.grid(linestyle='--')


fig, axs = plt.subplots(2, 3, figsize=(24, 12))
peStr = '-pe' + "{:.1f}".format(0.0)
plot_gpt_stacks(axs[0][0], 5, 0.06, peStr)
axs[0][0].set_title('(a) RTprop=60ms, Pe=0%', y=1.0, fontsize=14)

peStr = '-pe' + "{:.3f}".format(0.001)
plot_gpt_stacks(axs[0][1], 5, 0.06, peStr)
axs[0][1].set_title('(b) RTprop=60ms, Pe=0.1%', y=1.0, fontsize=14)


peStr = '-pe' + "{:.2f}".format(0.01)
plot_gpt_stacks(axs[0][2], 5, 0.06, peStr)
axs[0][2].set_title('(c) RTprop=60ms, Pe=1%', y=1.0, fontsize=14)

peStr = '-pe' + "{:.1f}".format(0.0)
plot_gpt_stacks(axs[1][0], 5, 0.6, peStr)
axs[1][0].set_title('(d) RTprop=600ms, Pe=0%', y=1.0, fontsize=14)
axs[1][0].set_xlabel('Number of Pairs',fontsize=12)
axs[1][0].xaxis.set_minor_formatter(FuncFormatter(formatterFunc))

peStr = '-pe' + "{:.3f}".format(0.001)
plot_gpt_stacks(axs[1][1], 5, 0.6, peStr)
axs[1][1].set_title('(e) RTprop=600ms, Pe=0.1%', y=1.0, fontsize=14)
axs[1][1].set_xlabel('Number of Pairs',fontsize=12)
axs[1][1].xaxis.set_minor_formatter(FuncFormatter(formatterFunc))

peStr = '-pe' + "{:.2f}".format(0.01)
plot_gpt_stacks(axs[1][2], 5, 0.6, peStr)
axs[1][2].set_title('(f) RTprop=600ms, Pe=1%', y=1.0, fontsize=14)
axs[1][2].set_xlabel('Number of Pairs',fontsize=12)
axs[1][2].xaxis.set_minor_formatter(FuncFormatter(formatterFunc))

#plt.savefig('overview.eps',bbox_inches='tight')

# Plot CoV
fig, axs = plt.subplots(1, 3, figsize=(12, 9))
fn = 10

peStr = '-pe' + "{:.1f}".format(0.0)
plot_CoV(axs[0], fn, 5, 0.6, peStr)
axs[0].set_title('(a) CoV of Pe=0%', y=1.0, fontsize=14)


peStr = '-pe' + "{:.3f}".format(0.001)
plot_CoV(axs[1], fn, 5, 0.6, peStr)
axs[1].set_title('(b) CoV of Pe=0.1%', y=1.0, fontsize=14)


peStr = '-pe' + "{:.2f}".format(0.01)
plot_CoV(axs[2], fn, 5, 0.6, peStr)
axs[2].set_title('(c) CoV of Pe=1%', y=1.0, fontsize=14)
#plt.savefig('cov.eps',bbox_inches='tight')


# Plot SFI
fig, axs = plt.subplots(1, 3, figsize=(12, 9))
fn = 10

peStr = '-pe' + "{:.1f}".format(0.0)
plot_SFI(axs[0], fn, 5, 0.6, peStr)
axs[0].set_title('(a) SFI of Pe=0%', y=1.0, fontsize=14)

peStr = '-pe' + "{:.3f}".format(0.001)
plot_SFI(axs[1], fn, 5, 0.6, peStr)
axs[1].set_title('(b) SFI of Pe=0.1%', y=1.0, fontsize=14)

peStr = '-pe' + "{:.2f}".format(0.01)
plot_SFI(axs[2], fn, 5, 0.6, peStr)
axs[2].set_title('(c) SFI of Pe=1%', y=1.0, fontsize=14)
#plt.savefig('sfi.eps',bbox_inches='tight')