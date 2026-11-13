# load_accounts.py PORT: stall_load's setup, 20,000 accounts in one RESTORE ?commit, into a node on 127.0.0.1:PORT.
import asyncio, os, sys

sys.path.insert(0, os.path.join(os.getcwd(), "harness"))
import stall_load  # noqa: E402

took = asyncio.run(stall_load.load_accounts("127.0.0.1", int(sys.argv[1]), stall_load.load_rows(), 20_000))
print(f"loaded 20000 accounts in {took:.2f} s")
