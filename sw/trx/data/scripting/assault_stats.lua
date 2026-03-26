local raw = trxc.assault_stats

trx.assault_stats = {
  add_record = raw.record,
  remove_record = raw.remove,
  list_records = raw.list,
}
