-- Kurali shared coffee mug — run in Supabase SQL Editor (PostgreSQL)
-- 1. Create project at https://supabase.com
-- 2. Run this whole script
-- 3. Settings → API: copy Project URL + anon public key into website/coffee.html (KURALI_MUG_*)
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

-- Add this table to the Realtime publication (safe to re-run).
do $$
begin
  if not exists (
    select 1
    from pg_publication_tables
    where pubname = 'supabase_realtime'
      and schemaname = 'public'
      and tablename = 'mug_state'
  ) then
    alter publication supabase_realtime add table public.mug_state;
  end if;
end $$;

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

-- If the browser error says "Could not find the function public.pour_coffee(...) in the schema cache":
-- 1. Run this entire file in the SAME Supabase project as KURALI_MUG_SUPABASE_URL in coffee.html.
-- 2. Then run the NOTIFY below (or Dashboard → Settings → API → Reload schema).
-- 3. Confirm the function exists:
--    select proname, pg_get_function_identity_arguments(oid)
--    from pg_proc p join pg_namespace n on n.oid = p.pronamespace
--    where n.nspname = 'public' and proname = 'pour_coffee';

notify pgrst, 'reload schema';

-- ---------------------------------------------------------------------------
-- Troubleshooting: error "column \"infinity\" does not exist" when pouring
-- pour_coffee updates public.mug_state. If you added a TRIGGER on that table,
-- fix the trigger function: Postgres may parse bare `infinity` as a column name.
-- Use a typed literal instead, e.g. 'infinity'::timestamptz, or fix the typo.
--
-- List triggers on mug_state:
--   select tgname, pg_get_triggerdef(t.oid, true) as definition
--   from pg_trigger t
--   join pg_class c on c.oid = t.tgrelid
--   join pg_namespace n on n.oid = c.relnamespace
--   where n.nspname = 'public' and c.relname = 'mug_state' and not t.tgisinternal;
-- ---------------------------------------------------------------------------
