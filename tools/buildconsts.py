import json
import glob
import os.path
import shutil
import collections


HEADER = """
// This file is generated automaticaly

#define ECHONET_LITE_DEFAULT_GET_PROPERTY_MAP 0x9d,0x9e,0x9f

typedef enum {
    SetI = 0x60,
    SetC = 0x61,
    Get = 0x62,
    INF_REQ = 0x63,
    SetGet = 0x64,
    Set_Res = 0x71,
    Get_Res = 0x72,

    INF = 0x73,
    INFC = 0x74,
    INFC_Res = 0x7A,
    SetGet_Res = 0x7E,

    SetI_SNA = 0x50,
    SetC_SNA = 0x51,
    Get_SNA = 0x52,
    INF_SNA = 0x53,
    SetGet_SNA = 0x5E,
} EchonetService;
"""


def capitalize(value):
    return value[:1].upper() + value[1:]


def main():
    import sys
    mra_data_path = sys.argv[1]

    with open("consts.h", "w") as outfile:
        # Header
        outfile.write(HEADER + "\r\n")

        # ObjectClass, EPC
        epc_dict = collections.OrderedDict()

        outfile.write("typedef enum {" + "\r\n")
        outfile.write("  EOJNodeProfile = 0x0EF0," + "\r\n")
        files = list(glob.glob(os.path.join(mra_data_path, "devices/*.json")))
        files.sort()
        for f in files:
            with open(f) as fp:
                device = json.load(fp)
                short_name = capitalize(device["shortName"])
                eoj = device["eoj"]
                outfile.write(
                    "  EOJ{} = {},".format(short_name, eoj) + "\r\n"
                )

                for prop in device["elProperties"]:
                    epc_name = "{}{}".format(short_name, capitalize(prop["shortName"]))
                    epc_dict[epc_name] = prop["epc"]


        outfile.write("} EchonetObjectClass;" + "\r\n")


        outfile.write("typedef enum {" + "\r\n")
        for k, v in epc_dict.items():
                outfile.write(
                    "  EPC{} = {},".format(k, v) + "\r\n"
                )
        outfile.write("} EchonetProperty;" + "\r\n")


    shutil.move("consts.h", "include/echonet_consts.h")

if __name__ == "__main__":
    main()






