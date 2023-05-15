DROP SCHEMA IF EXISTS service_schema CASCADE;

CREATE SCHEMA IF NOT EXISTS service_schema;

CREATE TABLE IF NOT EXISTS service_schema.couriers
(
    courier_id BIGINT PRIMARY KEY,
    courier_type TEXT,
    regions INTEGER [],
    working_hours TEXT [],
    completed_orders BIGINT [] default NULL
);

CREATE TABLE IF NOT EXISTS service_schema.orders
(
    order_id BIGINT PRIMARY KEY,
    weight NUMERIC,
    regions INTEGER,
    delivery_hours TEXT [],
    cost INTEGER,
    complete_time TIMESTAMP DEFAULT NULL
);

CREATE FUNCTION service_schema.CastTextToTimestamp(my_text TEXT) RETURNS TEXT as $$
select cast(CAST(my_text as TIMESTAMP) as TEXT);
$$
LANGUAGE SQL;

CREATE FUNCTION service_schema.CalculateTimestampDiffInSeconds(start_ TIMESTAMP, end_ TIMESTAMP) returns INTEGER as $$
select EXTRACT(EPOCH FROM end_) - EXTRACT(EPOCH FROM start_);
$$
LANGUAGE SQL;

CREATE FUNCTION service_schema.IsInDateInterval(start_ TIMESTAMP, end_ TIMESTAMP, date TIMESTAMP) RETURNs BOOLEAN AS $$
select case
        WHEN (start_ <= date) and (date < end_)
            then TRUE
            else FALSE
        end
$$ LANGUAGE sql;

CREATE FUNCTION service_schema.ReturnCurrentDate() RETURNS TEXT as $$
select CAST((SELECT CURRENT_DATE) as TEXT);
$$ LANGUAGE sql;

CREATE FUNCTION service_schema.AddSomeMinutes(time_ TIMESTAMP, interval_ INTERVAL) RETURNS TEXT as $$
SELECT CAST((time_ +  interval_) as TEXT);
$$ LANGUAGE SQL;

