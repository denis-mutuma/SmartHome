-- Review existing rows before applying:
-- select id, room_id, name from public.devices
-- where kind = 'thermometer' and celsius is null;
-- Resolve any returned rows explicitly; this migration does not invent readings.

do $$
begin
  if exists (
    select 1
    from public.devices
    where kind = 'thermometer' and celsius is null
  ) then
    raise exception 'Thermometers with null celsius exist; review and repair them before applying this migration';
  end if;
end;
$$;

alter table public.devices
  drop constraint if exists devices_check,
  drop constraint if exists devices_thermometer_shape_check;

alter table public.devices
  add constraint devices_thermometer_shape_check check (
    (
      kind = 'thermometer'
      and is_on is null
      and celsius is not null
      and celsius between 18.0 and 28.0
    )
    or (
      kind in ('light', 'plug')
      and celsius is null
      and reading_at is null
      and is_on is not null
    )
  );