insert into flags (name, hint, value, points, created_at, updated_at) values
  ('Dive Right In', 'Find the device''s serial number. Flag format: flag{SERIAL}', 'CT-923456A', 25, now(), now()),
  ('Fish Fry', 'Find the device''s processor/module serial number. Flag format: flag{SERIAL-SERIAL-SERIAL}', 'ESP32--WROOM-320', 35, now(), now()),
  ('Catch of the Day', 'Find the device''s firmware version string. Flag format: flag{version_string}', 'COOLTUNA v1.34.5.1.2', 50, now(), now()),
  ('Cast Your Nets', 'Find the device''s wireless access point SSID. Flag format: flag{SSID}', 'interceptctfnet', 75, now(), now()),
  ('MFTUNA', 'Bypass COOLTUNAs on-boot security mechanisms.', 'PLACEHOLDER', 50, now(), now()),
  ('MACkerel Fishing', 'Find your device''s MAC address and extract the OUI. Flag format: flag{0123AB} (uppercase hex!)', '246F28', 50, now(), now());
