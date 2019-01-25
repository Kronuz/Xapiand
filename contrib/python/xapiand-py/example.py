from datetime import datetime
from xapiand import Xapiand

client = Xapiand(
    url_prefix='production',
    # # sniff before doing anything
    # sniff_on_start=True,
    # # refresh nodes after a node fails to respond
    # sniff_on_connection_fail=True,
    # # and also every 60 seconds
    # sniffer_timeout=60,
)

response = client.index(index="my-index6", id=42, body={"any": "data", "timestamp": datetime.now()}, commit=True)
print(response)

response = client.get(index="my-index6", id=42)
print(response)
