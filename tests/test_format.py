"""在临时目录验证格式化脚本，不批量改动工程源码。"""

from pathlib import Path
import codecs
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SHELLS = [path for name in ("pwsh", "powershell") if (path := shutil.which(name))]
SOURCE = '// 长度和校验，保留中文注释\nint main(void){return 0;}\n'


@unittest.skipUnless(SHELLS and shutil.which("clang-format"), "需要 PowerShell 和 clang-format")
class FormatTests(unittest.TestCase):
    def fixture(self, directory, data):
        project = Path(directory) / "中文 工程"
        (project / "app").mkdir(parents=True)
        shutil.copy2(ROOT / "bootloader/format.ps1", project / "format.ps1")
        shutil.copy2(ROOT / "bootloader/.clang-format", project / ".clang-format")
        source = project / "app/sample.c"
        source.write_bytes(data)
        return project, source

    def run_format(self, shell, project):
        return subprocess.run(
            [shell, "-NoProfile", "-File", str(project / "format.ps1"), "-Paths", "app"],
            capture_output=True,
            timeout=30,
        )

    def test_preserves_encoding_text_and_second_run(self):
        for shell in SHELLS:
            for encoding in ("gbk", "utf-8", "utf-8-sig"):
                with self.subTest(shell=shell, encoding=encoding), tempfile.TemporaryDirectory() as directory:
                    project, source = self.fixture(directory, SOURCE.encode(encoding))
                    result = self.run_format(shell, project)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    first = source.read_bytes()
                    decoded = first.decode(encoding)
                    self.assertIn("长度和校验，保留中文注释", decoded)
                    self.assertNotIn("\ufffd", decoded)
                    self.assertEqual(first.startswith(codecs.BOM_UTF8), encoding == "utf-8-sig")
                    self.assertTrue(decoded.endswith("\n\n"))
                    self.assertIn("    return 0;", decoded)
                    self.assertEqual(self.run_format(shell, project).returncode, 0)
                    self.assertEqual(source.read_bytes(), first)

    def test_invalid_encoding_does_not_overwrite(self):
        for shell in SHELLS:
            with self.subTest(shell=shell), tempfile.TemporaryDirectory() as directory:
                original = b"// \x81\nint value;\n"
                project, source = self.fixture(directory, original)
                result = self.run_format(shell, project)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(source.read_bytes(), original)

    def test_formatter_failure_does_not_overwrite(self):
        for shell in SHELLS:
            with self.subTest(shell=shell), tempfile.TemporaryDirectory() as directory:
                original = SOURCE.encode("gbk")
                project, source = self.fixture(directory, original)
                (project / ".clang-format").write_text("UnknownStyleKey: true\n", encoding="utf-8")
                result = self.run_format(shell, project)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(source.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
