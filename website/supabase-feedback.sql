-- Coral website feedback — run in the SAME Supabase project that powers the
-- coffee mug (CORAL_MUG_SUPABASE_URL in coffee.html / index.html).
-- 1. Paste this whole file into the Supabase SQL Editor.
-- 2. Wire an email webhook: Database → Webhooks → table "feedback", event INSERT
--    → point at Zapier/Make/n8n → Gmail or Resend. anon cannot SELECT this table,
--    so email is the delivery channel.

-- ---------------------------------------------------------------------------
-- Table
-- ---------------------------------------------------------------------------
create table if not exists public.feedback (
  id                bigserial primary key,
  created_at        timestamptz not null default now(),
  email             text,
  hear_from         text,
  use_case          text,
  comments          text,
  install_platform  text,  -- free-form tag: "web", "linux", "win32", "macos", etc.
  user_agent        text,
  client_token      uuid,  -- browser-generated UUID, lets you de-dup per visitor
  constraint feedback_email_len       check (email            is null or char_length(email)            <=  254),
  constraint feedback_hear_from_len   check (hear_from        is null or char_length(hear_from)        <=  500),
  constraint feedback_use_case_len    check (use_case         is null or char_length(use_case)         <= 2000),
  constraint feedback_comments_len    check (comments         is null or char_length(comments)         <= 4000),
  constraint feedback_platform_len    check (install_platform is null or char_length(install_platform) <=   32),
  constraint feedback_user_agent_len  check (user_agent       is null or char_length(user_agent)       <=  512)
);

create index if not exists feedback_created_at_idx on public.feedback (created_at desc);
create index if not exists feedback_client_token_idx on public.feedback (client_token);

-- ---------------------------------------------------------------------------
-- RPC: anon cannot INSERT directly; they call this function instead.
-- The function trims inputs, rejects fully-empty submissions, caps size via
-- table constraints above, and returns the new row id.
-- ---------------------------------------------------------------------------
create or replace function public.submit_feedback(
  p_email             text default null,
  p_hear_from         text default null,
  p_use_case          text default null,
  p_comments          text default null,
  p_install_platform  text default null,
  p_user_agent        text default null,
  p_client_token      uuid default null
)
returns json
language plpgsql
security definer
set search_path = public
as $$
declare
  v_id bigint;
  v_email text     := nullif(trim(p_email), '');
  v_hear  text     := nullif(trim(p_hear_from), '');
  v_use   text     := nullif(trim(p_use_case), '');
  v_cmts  text     := nullif(trim(p_comments), '');
begin
  -- Reject completely empty submissions.
  if v_email is null and v_hear is null and v_use is null and v_cmts is null then
    raise exception 'empty submission' using errcode = '22023';
  end if;

  -- Very-light email shape check; we still accept NULL.
  if v_email is not null and v_email !~* '^[^@\s]+@[^@\s]+\.[^@\s]+$' then
    raise exception 'invalid email' using errcode = '22023';
  end if;

  insert into public.feedback (
    email, hear_from, use_case, comments,
    install_platform, user_agent, client_token
  )
  values (
    v_email, v_hear, v_use, v_cmts,
    nullif(trim(p_install_platform), ''),
    nullif(trim(p_user_agent), ''),
    p_client_token
  )
  returning id into v_id;

  return json_build_object('ok', true, 'id', v_id);
end;
$$;

-- ---------------------------------------------------------------------------
-- Permissions & RLS
-- ---------------------------------------------------------------------------
revoke all on table public.feedback from public;
revoke all on function public.submit_feedback(text,text,text,text,text,text,uuid) from public;
grant execute on function public.submit_feedback(text,text,text,text,text,text,uuid) to anon, authenticated;

alter table public.feedback enable row level security;
-- No SELECT / INSERT / UPDATE / DELETE policy is granted to anon.
-- Access the data through the Supabase dashboard (service role) or via a webhook.

notify pgrst, 'reload schema';

-- ---------------------------------------------------------------------------
-- (Optional) basic per-visitor cap — uncomment to limit submissions from one
-- client_token to 3 per day. Prevents a single tab from spamming.
-- ---------------------------------------------------------------------------
-- create or replace function public.submit_feedback(
--   p_email text, p_hear_from text, p_use_case text, p_comments text,
--   p_install_platform text, p_user_agent text, p_client_token uuid
-- ) returns json language plpgsql security definer set search_path = public as $$
-- declare
--   v_id bigint; v_count int;
-- begin
--   if p_client_token is not null then
--     select count(*) into v_count from public.feedback
--       where client_token = p_client_token
--         and created_at > now() - interval '24 hours';
--     if v_count >= 3 then
--       raise exception 'rate limited' using errcode = '42501';
--     end if;
--   end if;
--   -- ... same insert body as the function above ...
-- end;
-- $$;

-- ---------------------------------------------------------------------------
-- Email notification via Supabase Webhook
-- ---------------------------------------------------------------------------
-- Database → Webhooks → Create:
--   Name: feedback-email
--   Table: feedback
--   Events: INSERT
--   Type: HTTP Request (POST)
--   URL:  your Zapier / Make / n8n webhook URL
--   Headers: { "Content-Type": "application/json" }
-- Zapier/Make step: "Send Email" via Gmail or Resend with fields from record.*
-- Subject suggestion: [Coral feedback] {{record.email}} ({{record.install_platform}})
