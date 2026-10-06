-- Home schema for the Supabase SQL editor.
-- Email confirmation must be off so signup returns a session.

create table public.profiles (
  id uuid primary key references auth.users (id) on delete cascade,
  first_name text not null check (char_length(first_name) between 1 and 40),
  city text check (city is null or char_length(city) between 1 and 80)
);

create table public.rooms (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null default auth.uid() references auth.users (id) on delete cascade,
  name text not null check (char_length(name) between 1 and 40),
  position integer not null check (position >= 0),
  unique (user_id, position)
);

create table public.devices (
  id uuid primary key default gen_random_uuid(),
  room_id uuid not null references public.rooms (id) on delete cascade,
  user_id uuid not null default auth.uid() references auth.users (id) on delete cascade,
  name text not null check (char_length(name) between 1 and 40),
  kind text not null check (kind in ('light', 'plug', 'thermometer')),
  is_on boolean,
  celsius numeric(4, 1),
  reading_at timestamptz,
  position integer not null check (position >= 0),
  unique (room_id, position),
  constraint devices_thermometer_shape_check check (
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
  )
);

create index devices_user_id_idx on public.devices (user_id);

alter table public.profiles enable row level security;
alter table public.rooms enable row level security;
alter table public.devices enable row level security;

alter table public.profiles force row level security;
alter table public.rooms force row level security;
alter table public.devices force row level security;

create policy profiles_select on public.profiles
  for select to authenticated
  using (id = auth.uid());

create policy profiles_update on public.profiles
  for update to authenticated
  using (id = auth.uid())
  with check (id = auth.uid());

create policy rooms_select on public.rooms
  for select to authenticated
  using (user_id = auth.uid());

create policy rooms_insert on public.rooms
  for insert to authenticated
  with check (user_id = auth.uid());

create policy rooms_update on public.rooms
  for update to authenticated
  using (user_id = auth.uid())
  with check (user_id = auth.uid());

create policy rooms_delete on public.rooms
  for delete to authenticated
  using (user_id = auth.uid());

create policy devices_select on public.devices
  for select to authenticated
  using (user_id = auth.uid());

create policy devices_insert on public.devices
  for insert to authenticated
  with check (
    user_id = auth.uid()
    and exists (
      select 1 from public.rooms
      where rooms.id = room_id and rooms.user_id = auth.uid()
    )
  );

create policy devices_update on public.devices
  for update to authenticated
  using (user_id = auth.uid())
  with check (
    user_id = auth.uid()
    and exists (
      select 1 from public.rooms
      where rooms.id = room_id and rooms.user_id = auth.uid()
    )
  );

create policy devices_delete on public.devices
  for delete to authenticated
  using (user_id = auth.uid());

revoke all on public.profiles from anon, public;
revoke all on public.rooms from anon, public;
revoke all on public.devices from anon, public;

grant select, update on public.profiles to authenticated;
grant select, insert, update, delete on public.rooms to authenticated;
grant select, insert, update, delete on public.devices to authenticated;

create function public.handle_new_user()
returns trigger
language plpgsql
security definer
set search_path = public
as $$
begin
  insert into public.profiles (id, first_name)
  values (new.id, new.raw_user_meta_data ->> 'first_name');
  return new;
end;
$$;

revoke all on function public.handle_new_user() from public, anon, authenticated;

create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();
