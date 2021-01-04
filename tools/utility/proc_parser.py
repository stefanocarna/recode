import os

PATH_PROC_RECODE = "/proc/recode/"
PATH_PROC_CPUS = PATH_PROC_RECODE + "cpus/"

EVT_LIST = []

# path = PATH_PROC_CPU


def read_proc_cpu(path=None):
    if path is None:
        print("A path is required")
        return []

    proc_cpu = open(path, "r")

    lines = []

    for line in proc_cpu.readlines():
        if line.startswith("#"):
            if len(EVT_LIST) == 0:
                EVT_LIST.extend([i for i in line[1:].strip().split(",")])
        else:
            lines.append([int(i) for i in line.strip().split(",")])

    proc_cpu.close()

    if len(lines) > 0:
        print("Found labels: " + str(EVT_LIST))
    else:
        print(path, "is empty")

    return lines


def fast_sort_cpu_list(data):
    added = True
    sorted_list = []
    min_time = -1

    min = (0, int(data[0][0][1]) + 1)

    while added:
        added = False

        for i in range(0, len(data)):
            if len(data[i]) == 0:
                continue
            # print("[" + str(i) + "] min: " + str(min) +
            #       " el: " + str(data[i][0][1]))
            if min[1] > int(data[i][0][1]):
                min = (i, int(data[i][0][1]))
                added = True

        if added:
            if min_time == -1:
                min_time = data[min[0]][0][1]
            sorted_list.append(data[min[0]].pop(0))
            sorted_list[-1][1] = sorted_list[-1][1] - min_time

        # Select next min
        for i in range(0, len(data)):
            if len(data[i]) == 0:
                continue
            min = (i, int(data[i][0][1]) + 1)
            break

        # print("---\n")

    return sorted_list


# def _get_nr_cpus():


def traslate_data(data, size):
    if len(data) == 0:
        return data

    #
    # Here data is formatted as follow:
    # data[0] = [v0_t0, v1_t0, v2_t0, ..., vn_t0]
    #           [v0_t1, v1_t1, v2_t1, ..., vn_t1]
    #           ...
    #           [v0_tm, v1_tm, v2_tm, ..., vn_tm]
    # data[1] = ...
    # data[k] = ...
    #
    # We want to get:
    # data[0] = [v0_t0, v0_t1, v0_t2, ..., v0_tm]
    #           [v1_t0, v1_t1, v1_t2, ..., v1_tm]
    #

    tr_data = []

    for i in data:
        tr_data.append([])
        for k in range(0, size):
            tr_data[-1].append([])
            for j in range(0, len(i)):
                tr_data[-1][-1].append(i[j][k])

    return tr_data


def read_data():

    if not os.path.isdir(PATH_PROC_CPUS):
        print("Cannot open " + PATH_PROC_CPUS)
        exit(0)

    cpus_data = []

    with os.scandir(PATH_PROC_CPUS) as entries:
        for entry in entries:
            print("Reading data from: " + str(entry.path))
            if not entry.name.startswith("cpu"):
                print("Found unknown file: " + entry.name + "... Skip")
            else:
                cpu_data = read_proc_cpu(entry.path)
                if len(cpu_data) != 0:
                    cpus_data.append(cpu_data)

    return EVT_LIST, cpus_data

    # cpus_data = []

    # # 4 is hardcoded for this machine which has 4 active CPUs
    # for i in range(0, 4):
    #         single_cpus_data = []
    #         single_cpus_data.append(read_proc_cpu(i, path))
    #         cpus_data.append(fast_sort_cpu_list(single_cpus_data))

    # print("Sample data: " + str(cpus_data[2][0]))
    # print("Sample data: " + str(cpus_data[2][1]))
    # print("Sample data: " + str(cpus_data[2][2]))
    # print("Sample data: " + str(cpus_data[2][3]))

    # # We have data formatted this way:
    # #   A list of each dataset to be plotted
    # #   The dataset structure:
    # #       [0][...] = tsc
    # #       [1][...] = value
    # #       [-2] avg value
    # #       [-1] 'label'

    # return EVT_LIST, cpus_data


def read_data_dict():
    labels, data = read_data()

    data = traslate_data(data, len(labels))

    data_dicts = []

    for i in range(0, len(data)):
        data_dicts.append({})
        for k in range(0, len(labels)):
            data_dicts[i][labels[k]] = data[i][k]

    return data_dicts


if __name__ == "__main__":
    read_data()
