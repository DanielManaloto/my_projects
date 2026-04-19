from enum import IntEnum, IntFlag
from array import array
import sys
import msvcrt
import struct
import signal


class Register(IntEnum):
    R_R0 = 0
    R_R1 = 1
    R_R2 = 2
    R_R3 = 3
    R_R4 = 4
    R_R5 = 5
    R_R6 = 6
    R_R7 = 7
    R_PC = 8      # program counter
    R_COND = 9
    R_COUNT = 10


class Opcode(IntEnum):
    OP_BR = 0     # branch
    OP_ADD = 1    # add
    OP_LD = 2     # load
    OP_ST = 3     # store
    OP_JSR = 4    # jump register
    OP_AND = 5    # bitwise and
    OP_LDR = 6    # load register
    OP_STR = 7    # store register
    OP_RTI = 8    # unused
    OP_NOT = 9    # bitwise not
    OP_LDI = 10   # load indirect
    OP_STI = 11   # store indirect
    OP_JMP = 12   # jump
    OP_RES = 13   # reserved (unused)
    OP_LEA = 14   # load effective address
    OP_TRAP = 15  # execute trap


class ConditionFlags(IntFlag):
    FL_POS = 1 << 0  # P
    FL_ZRO = 1 << 1  # Z
    FL_NEG = 1 << 2  # N


class TRAPCodes(IntEnum):
    TRAP_GETC = 0x20   # get character from keyboard, not echoed
    TRAP_OUT = 0x21    # output a character
    TRAP_PUTS = 0x22   # output a word string
    TRAP_IN = 0x23     # get character from keyboard, echoed
    TRAP_PUTSP = 0x24  # output a byte string
    TRAP_HALT = 0x25   # halt the program


class MMR(IntEnum): # Memmory Mapped Register
    MR_KBSR = 0xFE00  # keyboard status
    MR_KBDR = 0xFE02  # keyboard data

MEMORY_MAX = 1 << 16
memory = array('H', [0]) * MEMORY_MAX  # unsigned short (16-bit)
reg = array('H', [0]) * Register.R_COUNT

def update_flags(r):
    if reg[r] == 0:
        reg[Register.R_COND] = ConditionFlags.FL_ZRO
    elif (reg[r] >> 15):  
        reg[Register.R_COND] = ConditionFlags.FL_NEG
    else:
        reg[Register.R_COND] = ConditionFlags.FL_POS

def sign_extend(x, bit_count):
    if x >> (bit_count - 1) & 1:
        x |= (0xFFFF << bit_count)
    return x & 0xFFFF

def swap16(x):
    return (x << 8) | (x >> 8)
    
def read_image_file(file):
    header = file.read(2)
    if not header:
        return False
    
    origin = struct.unpack('>H', header)[0]
    data = file.read()
    read_count = len(data) // 2
    
    words = struct.unpack('>' + 'H' * read_count, data)
    
    for i, word in enumerate(words):
        if origin + i < MEMORY_MAX:
            memory[origin + i] = word
    
    return True

def read_image(image_path):
    try:
        with open(image_path, 'rb') as f:
            return read_image_file(f)
    except FileNotFoundError:
        return False

def disable_input_buffering():
    pass

def restore_input_buffering():
    pass

def handle_interrupt(signum, frame):
    restore_input_buffering()
    print("\n")
    sys.exit(-2)

def check_key():
    return msvcrt.kbhit()

def mem_read(address):
    address &= 0xFFFF
    if address == MMR.MR_KBSR:
        if check_key():
            memory[MMR.MR_KBSR] = 1 << 15
        else:
            memory[MMR.MR_KBSR] = 0
    elif address == MMR.MR_KBDR:
        if check_key():
            memory[MMR.MR_KBDR] = ord(msvcrt.getch())
        else:
            memory[MMR.MR_KBDR] = 0

    return memory[address]

def mem_write(address, value):
    address &= 0xFFFF
    value &= 0xFFFF
    memory[address] = value
    
def op_br(instr):
    pc_offset = sign_extend(instr & 0x1FF, 9)
    cond_flag = (instr >> 9) & 0x7
    if cond_flag & reg[Register.R_COND]:
        reg[Register.R_PC] = (reg[Register.R_PC] + pc_offset) & 0xFFFF
    else:
        pass

def op_add(instr):
    # Destination Register (dr)
    dr = (instr >> 9) & 0x7
    # Soure Register (sr)
    sr1 = (instr >> 6) & 0x7
    imm_flag = (instr >> 5) & 1

    if (imm_flag):
        imm5 = sign_extend(instr & 0x1F, 5)
        reg[dr] = (reg[sr1] + imm5) & 0xFFFF
    else:
        sr2 = instr & 0x7
        reg[dr] = (reg[sr1] + reg[sr2]) & 0xFFFF

    update_flags(dr)


def op_ld(instr):
    dr = (instr >> 9) & 0x7
    pc_offset = sign_extend(instr & 0x1FF, 9)
    reg[dr] = mem_read(reg[Register.R_PC] + pc_offset)
    update_flags(dr)
    pass

def op_st(instr):
    dr = (instr >> 9) & 0x7
    pc_offset = sign_extend(instr & 0x1FF, 9)
    mem_write(reg[Register.R_PC] + pc_offset, reg[dr])
    pass

def op_jsr(instr):
    long_flag = (instr >> 11) & 1
    reg[Register.R_R7] = reg[Register.R_PC]
    if long_flag:
        long_pc_offset = sign_extend(instr & 0x7FF, 11)
        reg[Register.R_PC] = (reg[Register.R_PC] + long_pc_offset) & 0xFFFF
    else:
        # Base Register (br)
        br = (instr >> 6) & 0x7
        reg[Register.R_PC] = reg[br]
    pass

def op_and(instr):
    dr = (instr >> 9) & 0x7
    sr1 = (instr >> 6) & 0x7
    imm_flag = (instr >> 5) & 0x1

    if imm_flag:
        imm5 = sign_extend(instr & 0x1F, 5)
        reg[dr] = (reg[sr1] & imm5) & 0xFFFF
    else:
        sr2 = instr & 0x7
        reg[dr] = (reg[sr1] & reg[sr2]) & 0xFFFF
    
    update_flags(dr)
    pass

def op_ldr(instr):
    dr = (instr >> 9) & 0x7
    br = (instr >> 6) & 0x7
    offset = sign_extend(instr & 0x3F, 6)
    reg[dr] = mem_read(reg[br] + offset)
    update_flags(dr)
    pass

def op_str(instr):
    dr = (instr >> 9) & 0x7
    br = (instr >> 6) & 0x7
    offset = sign_extend(instr & 0x3F, 6)
    mem_write(reg[br] + offset, reg[dr])
    pass

def op_rti(instr):    
    pass

def op_not(instr):
    dr = (instr >> 9) & 0x7
    sr = (instr >> 6) & 0x7

    reg[dr] = (~reg[sr]) & 0xFFFF
    update_flags(dr)
    pass

def op_ldi(instr):
    #destination register (DR)
    dr = (instr >> 9) & 0x7
    # PCoffset 9
    pc_offset = sign_extend(instr & 0x1FF, 9)
    reg[dr] = mem_read(mem_read(reg[Register.R_PC] + pc_offset))
    update_flags(dr)
    pass

def op_sti(instr):
    br = (instr >> 9) & 0x7
    pc_offset = sign_extend(instr & 0x1FF, 9)
    mem_write(mem_read(reg[Register.R_PC] + pc_offset), reg[br])
    pass

def op_jmp(instr):
    r1 = (instr >> 6) & 0x7
    reg[Register.R_PC] = reg[r1]
    pass
def op_res(instr):
    pass

def op_lea(instr):
    dr = (instr >> 9) & 0x7
    pc_offset = sign_extend(instr & 0x1FF, 9)
    reg[dr] = (reg[Register.R_PC] + pc_offset) & 0xFFFF
    update_flags(dr)
    pass

def op_trap(instr):
    reg[Register.R_R7] = reg[Register.R_PC]
    
    # Get the last 8 bits of the instruction to find the trap vector
    trap_vector = instr & 0xFF

    if trap_vector == TRAPCodes.TRAP_GETC:
        c = msvcrt.getch()
        reg[Register.R_R0] = ord(c)
        update_flags(Register.R_R0)

    elif trap_vector == TRAPCodes.TRAP_OUT:
        c = reg[Register.R_R0] & 0xFF
        sys.stdout.write(chr(c))
        sys.stdout.flush()

    elif trap_vector == TRAPCodes.TRAP_PUTS:
        addr = reg[Register.R_R0]
        while memory[addr] != 0:
            sys.stdout.write(chr(memory[addr] & 0xFF))
            addr += 1
        sys.stdout.flush()

    elif trap_vector == TRAPCodes.TRAP_IN:
        # Prompt user, read char, echo it, store in R0
        sys.stdout.write("Enter a character: ")
        sys.stdout.flush()
        
        c = msvcrt.getch()
        
        sys.stdout.write(c.decode('utf-8'))
        sys.stdout.flush()
        
        reg[Register.R_R0] = ord(c)
        update_flags(Register.R_R0)

    elif trap_vector == TRAPCodes.TRAP_PUTSP:
        # Output a packed string (two chars per word)
        addr = reg[Register.R_R0]
        while memory[addr] != 0:
            val = memory[addr]
            
            # Low byte (first char)
            char1 = val & 0xFF
            sys.stdout.write(chr(char1))
            
            # High byte (second char)
            char2 = val >> 8
            if char2:
                sys.stdout.write(chr(char2))
                
            addr += 1
        sys.stdout.flush()

    elif trap_vector == TRAPCodes.TRAP_HALT:
        print("HALT")
        sys.stdout.flush()
        sys.exit(0)

OPCODE_DISPATCH = {
    Opcode.OP_BR: op_br,
    Opcode.OP_ADD: op_add,
    Opcode.OP_LD: op_ld,
    Opcode.OP_ST: op_st,
    Opcode.OP_JSR: op_jsr,
    Opcode.OP_AND: op_and,
    Opcode.OP_LDR: op_ldr,
    Opcode.OP_STR: op_str,
    Opcode.OP_RTI: op_rti,
    Opcode.OP_NOT: op_not,
    Opcode.OP_LDI: op_ldi,
    Opcode.OP_STI: op_sti,
    Opcode.OP_JMP: op_jmp,
    Opcode.OP_RES: op_res,
    Opcode.OP_LEA: op_lea,
    Opcode.OP_TRAP: op_trap
}

def main():

    signal.signal(signal.SIGINT, handle_interrupt)

    if len(sys.argv) < 2:
        # show usage string
        print("lc3 [image-file1] ...")
        sys.exit(2)

    for filename in sys.argv[1:]:
        if not read_image(filename):
            print(f"failed to load image: {filename}")
            sys.exit(1)

    reg[Register.R_COND] = ConditionFlags.FL_ZRO

    PC_START = 0x3000
    reg[Register.R_PC] = PC_START

    running = True
    while running:
        instr = mem_read(reg[Register.R_PC])
        reg[Register.R_PC] += 1
        op = instr >> 12
        handler = OPCODE_DISPATCH.get(Opcode(op))
        if handler is None:
            raise RuntimeError(f"Unknown opcode: {op}")
            #sys.exit()
        handler(instr)

    restore_input_buffering()



if __name__ == "__main__":
    main()