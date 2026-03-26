trx.events.before_level_file(function(level)
  trx.creatures.add_ally(trx.catalog.objects.monk_2)
  trx.creatures.add_ally_target(trx.catalog.objects.bandit_2)
end)
