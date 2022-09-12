import json
import glob
import os.path
import shutil
import collections


HEADER = """
#ifndef _ECHONET_CONSTS_H_
#define _ECHONET_CONSTS_H_

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

typedef enum {
    LocationLiving = 0x08,
    LocationDinig = 0x10,
    LocationKitchen = 0x18,
    LocationBathroom = 0x20,
    LocationLavatory = 0x28,
    LocationWashroom = 0x30,
    LocationPassageway = 0x38,
    LocationRoom = 0x40,
    LocationStairway = 0x48,
    LocationFrontDoor = 0x50,
    LocationStoreroom = 0x58,
    LocationGarden = 0x60,
    LocationGarage = 0x68,
    LocationVeranda = 0x70,
    LocationOthers = 0x78,
    LocationIndefinite = 0xFF,
} EchonetLocation;

typedef enum {
    EchonetNoFault = 0x42,
    EchonetFault = 0x41,
} EchonetFaultStatus;

typedef enum {
    FaultNone = 0x0000,
    FaultColdReset = 0x0001,
    FaultReset = 0x0002,
    FaultSet = 0x0003,
    FaultSupply = 0x0004,
    FaultCleaning = 0x0005,
    FaultBattery = 0x0006,
    FaultUserDefined = 0x0009,
    FaultAbnormal = 0x000A, // 0x000A to 0x0013
    FaultSwitch = 0x0014,   // 0x0014 to 0x001D
    FaultSensor = 0x001E,   // 0x001E to 0x003B
    FaultComponent = 0x003C, // 0x003C to 0x0059
} EchonetFaultDescription;

"""

FOOTER = """
#endif /* _ECHONET_CONSTS_H_ */
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
        files = list(glob.glob(os.path.join(mra_data_path, "devices/*.json")))
        files.sort()
        files = [
            os.path.join(mra_data_path, "superClass/0x0000.json"),
            os.path.join(mra_data_path, "nodeProfile/0x0EF0.json"),
        ] + files
        for f in files:
            with open(f) as fp:
                device = json.load(fp)
                short_name = capitalize(device["shortName"])
                eoj = device["eoj"]
                outfile.write(
                    "  EOJ{} = {},".format(short_name, eoj) + "\r\n"
                )

                for prop in device["elProperties"]:
                    epc_name = "{}{}".format(short_name, capitalize(prop["shortName"])).replace("(", "").replace(")", "")
                    epc_dict[epc_name] = prop["epc"]


        outfile.write("} EchonetObjectClass;" + "\r\n\r\n")


        outfile.write("typedef enum {" + "\r\n")
        for k, v in epc_dict.items():
                outfile.write(
                    "  EPC{} = {},".format(k, v) + "\r\n"
                )
        outfile.write("} EchonetProperty;" + "\r\n\r\n")

        outfile.write(FOOTER + "\r\n")

    shutil.move("consts.h", "include/echonet_consts.h")

if __name__ == "__main__":
    main()






