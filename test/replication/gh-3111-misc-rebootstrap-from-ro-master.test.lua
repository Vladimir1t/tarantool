test_run = require('test_run').new()
test_run:cmd("restart server default")
uuid = require('uuid')

box.schema.user.grant('guest', 'replication')

-- gh-3111 - Allow to rebootstrap a replica from a read-only master
replica_uuid = uuid.new()
test_run:cmd('create server test with rpl_master=default, script="replication/replica_uuid.lua"')
test_run:cmd(string.format('start server test with args="%s"', replica_uuid))
test_run:cmd('stop server test')
test_run:cmd('cleanup server test')
box.cfg{read_only = true}
test_run:cmd(string.format('start server test with args="%s"', replica_uuid))
test_run:cmd('stop server test')
test_run:cmd('cleanup server test')
box.cfg{read_only = false}
test_run:cmd('delete server test')
test_run:cleanup_cluster()
box.schema.user.revoke('guest', 'replication')
