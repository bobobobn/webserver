-- 将目标键的值减1，如果目标键的值小于1，则返回0，否则返回减1后的值。
local kill_id = ARGV[1]
local kill_num = ARGV[2]
local order_id = ARGV[3]
print("kill_id:", kill_id, "kill_num:", kill_num, "order_id:", order_id)
local stream_name = "order.stream"
-- 库存id
local stock_key = "kill:stock:" .. kill_id
-- 订单id
local order_key = "kill:order:" .. kill_id
local stock = redis.call('get', stock_key)
if not stock then
    -- 库存不存在
    return -3
end
if (tonumber(stock) < 1) then
    -- 库存不足
    return -1
else
    if redis.call('sismember', order_key, order_id) == 1 then
        -- 订单已存在
        return -2
    end
end

-- 减库存, 增加订单
local stock = tonumber(redis.call('decrby', stock_key, tonumber(kill_num)))
redis.call('sadd', order_key, order_id)
-- 加消息队列
redis.call('XADD', stream_name, "*", "kill_id", kill_id, "kill_quantity", kill_num, "order_id", order_id)
return stock