-- Coral shared coffee mug — run in Supabase SQL Editor (PostgreSQL)
-- 1. Create project at https://supabase.com
-- 2. Run this whole script
-- 3. Settings → API: copy Project URL + anon public key into website/coffee.html (CORAL_MUG_*)
-- 4. Realtime: ensure mug_state is enabled (script adds it to publication)

create table if not exists public.mug_state (
  id smallint primary key default 1,
  fill_pct smallint not null default 0,
  full_cups bigint not null default 0,
  constraint mug_state_singleton check (id = 1),
  constraint fill_pct_range check (fill_pct >= 0 and fill_pct < 100)
);

insert into public.mug_state (id, fill_pct, full_cups)
values (1, 0, 0)
on conflict (id) do nothing;

-- Atomic pour: adds step%; each 100% increments full_cups and resets remainder.
-- step is capped so anonymous callers cannot wipe the counter in one call.
create or replace function public.pour_coffee(step int default 1)
returns json
language plpgsql
security definer
set search_path = public
as $$
declare
  f_before int;
  f int;
  c bigint;
  s int;
  crossed_30 boolean;
begin
  s := greatest(1, least(coalesce(step, 1), 5));

  select fill_pct, full_cups into f, c
  from public.mug_state
  where id = 1
  for update;

  if not found then
    insert into public.mug_state (id, fill_pct, full_cups) values (1, 0, 0);
    f := 0;
    c := 0;
  end if;

  f_before := f;
  crossed_30 := (f_before < 30) and ((f_before + s) >= 30);

  f := f + s;
  while f >= 100 loop
    f := f - 100;
    c := c + 1;
  end loop;

  update public.mug_state set fill_pct = f, full_cups = c where id = 1;

  return json_build_object(
    'fill_pct', f,
    'full_cups', c,
    'crossed_30', crossed_30
  );
end;
$$;

revoke all on public.mug_state from public;
grant select on public.mug_state to anon, authenticated;

revoke all on function public.pour_coffee(int) from public;
grant execute on function public.pour_coffee(int) to anon, authenticated;

alter table public.mug_state enable row level security;

drop policy if exists "Anyone can read mug_state" on public.mug_state;
create policy "Anyone can read mug_state"
  on public.mug_state for select
  using (true);

-- Realtime: other browsers see the same mug update live
alter table public.mug_state replica identity full;

-- Add this table to the Realtime publication (Publications UI will list 1 table for supabase_realtime).
-- If you re-run the whole script and this errors with "already a member", ignore that line.
alter publication supabase_realtime add table public.mug_state;

-- (Alternative) UI: Database → Publications → supabase_realtime → add table mug_state.

-- ---------------------------------------------------------------------------
-- Email when someone pours past 30% on the shared mug
-- Supabase Dashboard → Database → Webhooks → Create:
--   Table: mug_state, Events: UPDATE
--   Optional HTTP filter (or filter in Zapier/Make): only when fill_pct crosses 30.
--   Use old_record.fill_pct < 30 AND record.fill_pct >= 30
-- Point the webhook at Make / Zapier / n8n → Gmail (or Resend, etc.).
-- You cannot send email safely from static HTML alone; the webhook keeps secrets server-side.
-- ---------------------------------------------------------------------------
