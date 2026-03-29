import time
import pytest
from pytest_embedded_idf.dut import IdfDut

BURST_TIMEOUT_S = 60


def hard_reset(dut: IdfDut) -> None:
    dut.serial.proc.rts = True
    time.sleep(0.1)
    dut.serial.proc.rts = False
    time.sleep(0.5)


@pytest.mark.generic
def test_burst(dut: tuple[IdfDut, IdfDut]) -> None:
    sender, receiver = dut

    hard_reset(receiver)
    hard_reset(sender)

    receiver.expect('sx1278 init ok', timeout=10)
    sender.expect('sx1278 init ok',   timeout=10)

    sender.expect('burst sent',              timeout=BURST_TIMEOUT_S)
    receiver.expect('burst complete:',       timeout=BURST_TIMEOUT_S)
