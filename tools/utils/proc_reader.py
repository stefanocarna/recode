import time

PATH_PROC_RECODE = "/proc/recode/cpus/"


class ProcCpuReader:

    def __init__(self, cpuId="cpu0"):
        self.evtList = []
        self.dictLines = {}
        self.pFile = open(PATH_PROC_RECODE + cpuId, "r")

    def __read_cpu(self):
        """ Look for Event Labels """
        if (len(self.evtList) == 0):
            line = self.pFile.readline()
            if (line.startswith("#")):
                self.evtList.extend([i for i in line[1:].strip().split(",")])
                for k in self.evtList:
                    self.dictLines[k] = []
            else:
                print("Cannot find Event List, line:" + line)
                return 0

        """ Get the raw line and simultaneously build the dict """
        readLines = self.pFile.readlines()
        for line in readLines:
            for i, w in enumerate(line.strip().split(",")):
                self.dictLines[self.evtList[i]].append(int(w))

        return len(readLines)

    def try_read(self):
        if (self.__read_cpu() == 0):
            return None
        return self.dictLines

    def read(self):
        while (self.__read_cpu() == 0):
            time.sleep(1)

        return self.dictLines

    def close(self):
        self.pFile.close()
