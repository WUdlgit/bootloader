// 执行拆分后的真实 CRC、描述头和 Flash 模块；底层使用主机 Flash 模型。
const assert = require('node:assert/strict');
const fs = require('node:fs');
const { crc32 } = require('node:zlib');

(async () => {
    const { instance } = await WebAssembly.instantiate(fs.readFileSync(process.argv[2]));
    const c = instance.exports;
    const memory = new Uint8Array(c.memory.buffer);
    const view = new DataView(c.memory.buffer);
    const APP = 0x08010000, INFO = 0x0800c000, BUF = 0x20000;
    const check = (value, message) => assert.ok(value, message);
    let cases = 0;
    function test(name, fn) {
        c.test_reset();
        fn();
        assert.equal(c.test_locked(), 1, 'Flash must be locked on return');
        assert.equal(c.test_bad_access(), 0, 'No writes outside allowed regions');
        assert.equal(view.getUint32(0x08000000, true), 0x12345678);
        assert.equal(view.getUint32(0x08008000, true), 0x87654321);
        cases++;
        console.log('PASS ' + name);
    }
    function makeImage(length) {
        for (let i = 0; i < length; i++) memory[BUF + i] = i % 256;
        view.setUint32(BUF, 0x20020000, true);
        view.setUint32(BUF + 4, APP + 9, true);
        return crc32(memory.subarray(BUF, BUF + length));
    }
    function writeImage(length) {
        check(c.stm32_flash_erase(length), 'Erase must succeed');
        for (let offset = 0; offset < length; offset += 1024)
            check(c.stm32_flash_program(APP + offset, BUF + offset, Math.min(1024, length - offset)), 'Write/readback');
    }
    test('known CRC vector', () => {
        memory.set(Buffer.from('123456789'), BUF);
        assert.equal(c.crc32(BUF, 9) >>> 0, 0xcbf43926);
        assert.equal(c.crc32(BUF, 0) >>> 0, 0);
    });
    test('invalid lengths do not erase', () => {
        for (const length of [0, 7, 458753, 0xffffffff])
            assert.equal(c.stm32_flash_erase(length), 0);
        assert.equal(c.test_erased(), 0);
    });
    for (const length of [1023, 1024, 1025, 65535, 65536, 65537, 196607, 196608, 196609, 327679, 327680, 327681, 458752]) {
        test('write/commit/boot gate length ' + length, () => {
            const crc = makeImage(length);
            writeImage(length);
            assert.equal(c.magic_header_validate(), 0, 'No valid description before commit');
            const sectors = (1 << 3) | (1 << 4) | (length > 65536 ? 1 << 5 : 0) | (length > 196608 ? 1 << 6 : 0) | (length > 327680 ? 1 << 7 : 0);
            assert.equal(c.test_erased(), sectors);
            assert.deepEqual(memory.subarray(APP, APP + length), memory.subarray(BUF, BUF + length));
            assert.equal(c.test_image_crc_calls(), 0);
            check(c.magic_header_save(length, crc));
            assert.equal(c.test_image_crc_calls(), 1, '提交时只扫描一次 App');
            check(c.magic_header_validate());
            assert.equal(c.test_image_crc_calls(), 2, '启动检查必须重新扫描 App');
            memory[APP + length - 1] ^= 1;
            assert.equal(c.magic_header_validate(), 0, 'Corruption must block boot');
        });
    }
    for (let offset = 0; offset < 4; offset++) {
        for (let length = 1; length <= 9; length++) {
            test(`word programming alignment ${offset} length ${length}`, () => {
                for (let i = 0; i < length; i++) memory[BUF + 1 + i] = 0xa0 + i;
                check(c.stm32_flash_erase(32));
                check(c.stm32_flash_program(APP + offset, BUF + 1, length));
                assert.deepEqual(memory.subarray(APP + offset, APP + offset + length),
                    memory.subarray(BUF + 1, BUF + 1 + length));
                if (offset) assert.equal(memory[APP + offset - 1], 0xff);
                assert.equal(memory[APP + offset + length], 0xff);
            });
        }
    }
    test('wrong CRC cannot commit', () => {
        const crc = makeImage(1025);
        writeImage(1025);
        assert.equal(c.magic_header_save(1025, (crc ^ 1) >>> 0), 0);
        assert.equal(c.magic_header_validate(), 0);
    });
    test('description corruption and out-of-range length', () => {
        const crc = makeImage(1025);
        writeImage(1025);
        check(c.magic_header_save(1025, crc));
        memory[INFO + 12] ^= 1;
        assert.equal(c.magic_header_validate(), 0);
        view.setUint32(INFO + 4, 0xffffffff, true);
        view.setUint32(INFO + 12, crc32(memory.subarray(INFO, INFO + 12)), true);
        assert.equal(c.magic_header_validate(), 0, 'Reject length before reading image');
    });
    test('invalid vectors with a matching image CRC', () => {
        makeImage(1025);
        for (const [stack, entry] of [[0x20000000, APP + 9], [0x20020008, APP + 9],
            [0x2001fffc, APP + 9], [0x20020000, APP + 8], [0x20020000, APP + 1025]]) {
            view.setUint32(BUF, stack, true);
            view.setUint32(BUF + 4, entry, true);
            writeImage(1025);
            assert.equal(c.magic_header_save(1025, crc32(memory.subarray(BUF, BUF + 1025))), 0);
        }
    });
    for (const operation of [0, 1, 2, 3, 4]) {
        test('erase failure at operation ' + operation, () => {
            c.test_fail_after(operation);
            assert.equal(c.stm32_flash_erase(458752), 0);
            assert.equal(c.magic_header_validate(), 0);
        });
    }
    test('failed metadata erase preserves old App', () => {
        const crc = makeImage(1025);
        writeImage(1025);
        check(c.magic_header_save(1025, crc));
        c.test_fail_after(0);
        assert.equal(c.stm32_flash_erase(1025), 0);
        check(c.magic_header_validate());
    });
    test('new upgrade invalidates old description before App write', () => {
        const crc = makeImage(1025);
        writeImage(1025);
        check(c.magic_header_save(1025, crc));
        c.test_fail_after(1); // 描述头已擦除，随后 App 扇区擦除失败。
        assert.equal(c.stm32_flash_erase(1025), 0);
        assert.equal(c.magic_header_validate(), 0);
        c.test_fail_after(-1);
        writeImage(1025);
        check(c.magic_header_save(1025, crc), 'Full retry can recover');
    });
    test('write error and readback mismatch propagate', () => {
        makeImage(1025);
        check(c.stm32_flash_erase(1025));
        c.test_fail_after(5);
        assert.equal(c.stm32_flash_program(APP, BUF, 1025), 0);
        c.test_fail_after(-1);
        check(c.stm32_flash_erase(1025));
        c.test_drop_after(0);
        assert.equal(c.stm32_flash_program(APP, BUF, 1025), 0);
        assert.equal(c.magic_header_validate(), 0);
    });
    test('write range and unsigned overflow', () => {
        for (const [address, length] of [[APP - 1, 1], [APP + 458752, 1],
            [APP + 458751, 2], [APP, 0xffffffff], [0xfffffff0, 32], [APP, 0]])
            assert.equal(c.stm32_flash_program(address, BUF, length), 0);
    });
    for (let operation = 0; operation < 4; operation++) {
        test('interrupted description commit at word ' + operation, () => {
            const crc = makeImage(1025);
            writeImage(1025);
            c.test_fail_after(operation);
            assert.equal(c.magic_header_save(1025, crc), 0);
            assert.equal(c.magic_header_validate(), 0);
        });
    }
    test('description readback mismatch', () => {
        const crc = makeImage(1025);
        writeImage(1025);
        c.test_drop_after(0);
        assert.equal(c.magic_header_save(1025, crc), 0);
        assert.equal(c.magic_header_validate(), 0);
    });
    console.log(`${cases} firmware host cases passed (Flash API model; no board evidence)`);
})().catch(error => { console.error(error); process.exitCode = 1; });
