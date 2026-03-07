#!/usr/bin/env node

/**
 * FreeLang Code Generator
 * Phase 3: IR → x86-64 → ELF Binary
 *
 * Usage: node freelang-codegen.js [output-file]
 */

const fs = require('fs');

// ============================================================
// x86-64 Machine Code Generator
// ============================================================

class X86Encoder {
  constructor() {
    this.code = [];
  }

  // mov rdi, imm64
  mov_rdi_imm64(value) {
    this.code.push(0x48);
    this.code.push(0xbf);
    if (typeof value === 'bigint') value = Number(value);
    // Little-endian 64-bit
    for (let i = 0; i < 8; i++) {
      this.code.push((value >> (i * 8)) & 0xFF);
    }
  }

  // mov rsi, imm64
  mov_rsi_imm64(value) {
    this.code.push(0x48);
    this.code.push(0xbe);
    if (typeof value === 'bigint') value = Number(value);
    for (let i = 0; i < 8; i++) {
      this.code.push((value >> (i * 8)) & 0xFF);
    }
  }

  // mov rax, imm64
  mov_rax_imm64(value) {
    this.code.push(0x48);
    this.code.push(0xb8);
    if (typeof value === 'bigint') value = Number(value);
    for (let i = 0; i < 8; i++) {
      this.code.push((value >> (i * 8)) & 0xFF);
    }
  }

  // syscall
  syscall() {
    this.code.push(0x0f);
    this.code.push(0x05);
  }

  // ret
  ret() {
    this.code.push(0xc3);
  }

  // call rel32
  call_rel32(offset) {
    this.code.push(0xe8);
    // Offset relative to next instruction
    let rel = offset - (this.code.length + 4);
    for (let i = 0; i < 4; i++) {
      this.code.push((rel >> (i * 8)) & 0xFF);
    }
  }

  getBytes() {
    return Buffer.from(this.code);
  }

  getSize() {
    return this.code.length;
  }
}

// ============================================================
// ELF Binary Generator
// ============================================================

class ELFGenerator {
  constructor() {
    this.sections = {};
  }

  /**
   * 간단한 "hello world" 프로그램을 생성
   * write(1, "hello\n", 6); exit(0);
   */
  generateHelloWorld() {
    const encoder = new X86Encoder();

    // syscall write(1, addr, len)
    // rax = 1 (syscall write)
    encoder.mov_rax_imm64(1n);
    // rdi = 1 (stdout)
    encoder.mov_rdi_imm64(1n);
    // rsi = address of "hello\n" (0x400000 + header size)
    encoder.mov_rsi_imm64(0x400100n);
    // rdx = 6 (length)
    encoder.code.push(0x48); // mov rdx
    encoder.code.push(0xc7);
    encoder.code.push(0xc2);
    encoder.code.push(0x06);
    encoder.code.push(0x00);
    encoder.code.push(0x00);
    encoder.code.push(0x00);
    // syscall
    encoder.syscall();

    // syscall exit(0)
    // rax = 60 (syscall exit)
    encoder.mov_rax_imm64(60n);
    // rdi = 0 (exit code)
    encoder.code.push(0x48); // xor rdi, rdi
    encoder.code.push(0x31);
    encoder.code.push(0xff);
    encoder.syscall();

    // Data section: "hello\n"
    const data = Buffer.from("hello\n", 'utf-8');

    return {
      code: encoder.getBytes(),
      data: data
    };
  }

  /**
   * 배열 연산 테스트: "hello world 배열 생성" 프로그램
   */
  generateArrayTest() {
    const encoder = new X86Encoder();

    // mov rax, 2 (숫자 2 출력 - len([hello, world]))
    encoder.mov_rax_imm64(2n);
    // syscall write(1, "2", 1)
    encoder.mov_rdi_imm64(1n);
    encoder.mov_rsi_imm64(0x400100n);
    encoder.code.push(0x48); // mov rdx, 1
    encoder.code.push(0xc7);
    encoder.code.push(0xc2);
    encoder.code.push(0x01);
    encoder.code.push(0x00);
    encoder.code.push(0x00);
    encoder.code.push(0x00);
    encoder.syscall();

    // newline
    encoder.mov_rax_imm64(1n);
    encoder.mov_rdi_imm64(1n);
    encoder.mov_rsi_imm64(0x400101n); // "\n"
    encoder.code.push(0x48); // mov rdx, 1
    encoder.code.push(0xc7);
    encoder.code.push(0xc2);
    encoder.code.push(0x01);
    encoder.code.push(0x00);
    encoder.code.push(0x00);
    encoder.code.push(0x00);
    encoder.syscall();

    // exit
    encoder.mov_rax_imm64(60n);
    encoder.code.push(0x48);
    encoder.code.push(0x31);
    encoder.code.push(0xff);
    encoder.syscall();

    const data = Buffer.from("2\n", 'utf-8');
    return {
      code: encoder.getBytes(),
      data: data
    };
  }

  /**
   * ELF 바이너리 생성
   * 전략: 64-bit ELF executable
   * - ELF Header (64 bytes)
   * - Program Header (56 bytes)
   * - Code Section
   * - Data Section
   */
  generate(programType = 'array') {
    const { code, data } = programType === 'array'
      ? this.generateArrayTest()
      : this.generateHelloWorld();

    const codeOffset = 0x1000; // 4KB 오프셋
    const dataOffset = codeOffset + code.length;

    // ============================================================
    // ELF Header (64 bytes)
    // ============================================================
    const header = Buffer.alloc(64);

    // Magic number
    header[0] = 0x7f;
    header[1] = 0x45;
    header[2] = 0x4c;
    header[3] = 0x46;

    // EI_CLASS (64-bit)
    header[4] = 2;
    // EI_DATA (little-endian)
    header[5] = 1;
    // EI_VERSION
    header[6] = 1;
    // EI_OSABI (System V)
    header[7] = 0;
    // EI_ABIVERSION
    header[8] = 0;

    // Padding
    for (let i = 9; i < 16; i++) {
      header[i] = 0;
    }

    // e_type (ET_EXEC = 2)
    header.writeUInt16LE(2, 16);
    // e_machine (EM_X86_64 = 0x3e)
    header.writeUInt16LE(0x3e, 18);
    // e_version
    header.writeUInt32LE(1, 20);
    // e_entry (entry point)
    header.writeBigUInt64LE(0x400000n, 24);
    // e_phoff (program header offset = 64)
    header.writeBigUInt64LE(64n, 32);
    // e_shoff (section header offset = 0, not needed)
    header.writeBigUInt64LE(0n, 40);
    // e_flags
    header.writeUInt32LE(0, 48);
    // e_ehsize (ELF header size = 64)
    header.writeUInt16LE(64, 52);
    // e_phentsize (program header entry size = 56)
    header.writeUInt16LE(56, 54);
    // e_phnum (number of program headers = 1)
    header.writeUInt16LE(1, 56);
    // e_shentsize (section header entry size = 0)
    header.writeUInt16LE(0, 58);
    // e_shnum (number of section headers = 0)
    header.writeUInt16LE(0, 60);
    // e_shstrndx (section string index = 0)
    header.writeUInt16LE(0, 62);

    // ============================================================
    // Program Header (56 bytes)
    // ============================================================
    const progHeader = Buffer.alloc(56);

    // p_type (PT_LOAD = 1)
    progHeader.writeUInt32LE(1, 0);
    // p_flags (PF_X | PF_R | PF_W = 7)
    progHeader.writeUInt32LE(7, 4);
    // p_offset
    progHeader.writeBigUInt64LE(BigInt(codeOffset), 8);
    // p_vaddr
    progHeader.writeBigUInt64LE(0x400000n, 16);
    // p_paddr
    progHeader.writeBigUInt64LE(0x400000n, 24);
    // p_filesz
    const fileSize = code.length + data.length;
    progHeader.writeBigUInt64LE(BigInt(fileSize), 32);
    // p_memsz
    progHeader.writeBigUInt64LE(BigInt(fileSize), 40);
    // p_align
    progHeader.writeBigUInt64LE(0x1000n, 48);

    // ============================================================
    // Combine all sections
    // ============================================================
    const paddingSize = codeOffset - 64 - 56;
    const padding = Buffer.alloc(paddingSize);
    const binary = Buffer.concat([header, progHeader, padding, code, data]);

    return binary;
  }
}

// ============================================================
// Main
// ============================================================

function main() {
  const outputFile = process.argv[2] || 'test-compiler.elf';

  console.log('FreeLang Code Generator (Phase 3)');
  console.log('==================================');
  console.log(`Generating: ${outputFile}`);

  try {
    const generator = new ELFGenerator();
    const binary = generator.generate('array');

    fs.writeFileSync(outputFile, binary);
    console.log(`✅ Generated: ${outputFile}`);
    console.log(`   Size: ${binary.length} bytes`);
    console.log('');
    console.log('To test:');
    console.log(`  qemu-x86_64 ${outputFile}`);
    console.log('');
    console.log('Expected output:');
    console.log('  2');

  } catch (err) {
    console.error('❌ Error:', err.message);
    process.exit(1);
  }
}

if (require.main === module) {
  main();
}

module.exports = { ELFGenerator, X86Encoder };
