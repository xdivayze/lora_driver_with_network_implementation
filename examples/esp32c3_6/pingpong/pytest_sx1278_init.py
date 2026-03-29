import time
import pytest
from pytest_embedded_idf.dut import IdfDut


def hard_reset(dut: IdfDut) -> None:
    dut.serial.proc.rts = True
    time.sleep(0.1)
    dut.serial.proc.rts = False
    time.sleep(0.5)


@pytest.mark.generic
def test_sx1278_init(dut: IdfDut) -> None:
    hard_reset(dut)
    dut.expect('sx1278 init ok', timeout=10)
