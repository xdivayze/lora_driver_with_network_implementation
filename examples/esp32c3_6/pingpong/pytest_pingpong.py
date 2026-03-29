import time
import pytest
from pytest_embedded_idf.dut import IdfDut

ROUNDS = 5
ROUND_TIMEOUT_S = 15


def hard_reset(dut: IdfDut) -> None:
    dut.serial.proc.rts = True
    time.sleep(0.1)
    dut.serial.proc.rts = False
    time.sleep(0.5)


@pytest.mark.generic
def test_pingpong(dut: tuple[IdfDut, IdfDut]) -> None:
    sender, receiver = dut

    hard_reset(sender)
    hard_reset(receiver)

    for _ in range(ROUNDS):
        sender.expect('ping sent',       timeout=ROUND_TIMEOUT_S)
        receiver.expect('ping received', timeout=ROUND_TIMEOUT_S)
        receiver.expect('pong sent',     timeout=ROUND_TIMEOUT_S)
        sender.expect('pong received',   timeout=ROUND_TIMEOUT_S)

    sender.expect('test passed', timeout=10)
